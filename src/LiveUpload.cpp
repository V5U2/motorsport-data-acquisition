#include "LiveUpload.h"

#include <WiFi.h>
#include <esp_system.h>

#include "Logic.h"

namespace {

constexpr uint16_t kMqttBufferSize = 2048;

}  // namespace

LiveUpload::LiveUpload() : mqttClient_(networkClient_) {}

bool LiveUpload::begin(const AppConfig::UploadConfig &config, const bool enabled) {
  config_ = config;
  enabled_ = enabled;

  const std::string normalizedDevice = Logic::normalizeTopicSegment(config.deviceId);
  deviceId_ = normalizedDevice.empty() ? "mda-logger" : String(normalizedDevice.c_str());

  const uint32_t bootCounter = esp_random();
  sessionId_ = String(Logic::formatSessionId(deviceId_.c_str(), bootCounter).c_str());
  clientId_ = deviceId_ + "-" + String(static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFULL), HEX);

  mqttClient_.setServer(config_.mqttHost, config_.mqttPort);
  mqttClient_.setBufferSize(kMqttBufferSize);

  if (!enabled_) {
    lastError_ = "Live upload disabled";
    return false;
  }

  if (strlen(config_.mqttHost) == 0) {
    lastError_ = "MQTT host not configured";
    return false;
  }

  lastError_ = "";
  return true;
}

void LiveUpload::loop() {
  if (!enabled_) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    publishOfflineStatusAndDisconnect();
    if (lastError_.isEmpty()) {
      lastError_ = "Wi-Fi disconnected";
    }
    return;
  }

  mqttClient_.loop();
}

bool LiveUpload::publishIfDue(const AppState &state) {
  if (!enabled_) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {
    publishOfflineStatusAndDisconnect();
    lastError_ = "Wi-Fi disconnected";
    return false;
  }

  const uint32_t nowMs = millis();
  mqttClient_.loop();

  if (!mqttClient_.connected() && !reconnect(nowMs)) {
    return false;
  }

  if ((nowMs - lastPublishMs_) < config_.publishIntervalMs) {
    return false;
  }

  if (!publishSnapshot(state)) {
    return false;
  }

  lastPublishMs_ = nowMs;
  lastError_ = "";
  return true;
}

bool LiveUpload::isEnabled() const { return enabled_; }

bool LiveUpload::isConnected() { return mqttClient_.connected(); }

String LiveUpload::protocolName() const { return enabled_ ? "mqtt" : ""; }

String LiveUpload::sessionId() const { return sessionId_; }

String LiveUpload::lastError() const { return lastError_; }

uint32_t LiveUpload::lastSequence() const { return lastSequence_; }

bool LiveUpload::reconnect(const uint32_t nowMs) {
  if (strlen(config_.mqttHost) == 0) {
    lastError_ = "MQTT host not configured";
    return false;
  }

  if ((nowMs - lastReconnectAttemptMs_) < config_.reconnectIntervalMs) {
    return false;
  }

  lastReconnectAttemptMs_ = nowMs;

  bool connected = false;
  const String willPayload = buildStatusJson(false);
  if (strlen(config_.mqttUsername) > 0) {
    connected = mqttClient_.connect(clientId_.c_str(),
                                    config_.mqttUsername,
                                    config_.mqttPassword,
                                    statusTopic().c_str(),
                                    0,
                                    true,
                                    willPayload.c_str());
  } else {
    connected =
        mqttClient_.connect(clientId_.c_str(), statusTopic().c_str(), 0, true, willPayload.c_str());
  }

  if (!connected) {
    lastError_ = "MQTT connect failed rc=" + String(mqttClient_.state());
    return false;
  }

  if (!publishStatus(true)) {
    return false;
  }

  lastError_ = "";
  return true;
}

void LiveUpload::publishOfflineStatusAndDisconnect() {
  if (mqttClient_.connected()) {
    if (publishStatus(false)) {
      mqttClient_.disconnect();
    }
  }
}

bool LiveUpload::publishStatus(const bool connected) {
  const String payload = buildStatusJson(connected);
  if (!mqttClient_.publish(statusTopic().c_str(), payload.c_str(), true)) {
    lastError_ = "MQTT status publish failed";
    return false;
  }
  return true;
}

bool LiveUpload::publishSnapshot(const AppState &state) {
  const uint32_t sequence = lastSequence_ + 1;
  const String payload = buildSnapshotJson(state, sequence);
  if (!mqttClient_.publish(liveTopic().c_str(), payload.c_str(), false)) {
    lastError_ = "MQTT live publish failed";
    return false;
  }
  lastSequence_ = sequence;
  return true;
}

String LiveUpload::liveTopic() const {
  return String(Logic::formatUploadTopic(config_.topicPrefix, deviceId_.c_str(), "live").c_str());
}

String LiveUpload::statusTopic() const {
  return String(Logic::formatUploadTopic(config_.topicPrefix, deviceId_.c_str(), "status").c_str());
}

String LiveUpload::buildStatusJson(const bool connected) const {
  String json = "{";
  json += "\"device_id\":\"" + jsonEscape(deviceId_) + "\",";
  json += "\"session_id\":\"" + jsonEscape(sessionId_) + "\",";
  json += "\"protocol\":\"mqtt\",";
  json += "\"connected\":" + String(connected ? "true" : "false");
  json += "}";
  return json;
}

String LiveUpload::buildSnapshotJson(const AppState &state, const uint32_t sequence) const {
  String json = "{";
  json += "\"device_id\":\"" + jsonEscape(deviceId_) + "\",";
  json += "\"session_id\":\"" + jsonEscape(sessionId_) + "\",";
  json += "\"sequence\":" + String(sequence) + ",";
  json += "\"timestamp\":\"" + jsonEscape(state.timestamp) + "\",";
  json += "\"uptime_ms\":" + String(state.uptimeMs) + ",";
  json += "\"sensors\":[";
  for (size_t index = 0; index < state.sensors.size(); ++index) {
    if (index > 0) {
      json += ",";
    }
    const SensorSnapshot &sensor = state.sensors[index];
    json += "{";
    json += "\"id\":\"" + jsonEscape(sensor.id) + "\",";
    json += "\"name\":\"" + jsonEscape(sensor.name) + "\",";
    json += "\"value\":" + String(sensor.filteredValue, 3) + ",";
    json += "\"units\":\"" + jsonEscape(sensor.units) + "\",";
    json += "\"loop_mA\":" + String(sensor.loopCurrentmA, 3) + ",";
    json += "\"fault\":\"" + jsonEscape(sensorFaultToString(sensor.activeFault)) + "\"";
    json += "}";
  }
  json += "],";
  json += "\"system\":{";
  json += "\"adc_ready\":" + String(state.system.adcReady ? "true" : "false") + ",";
  json += "\"rtc_ready\":" + String(state.system.rtcReady ? "true" : "false") + ",";
  json += "\"sd_ready\":" + String(state.system.sdReady ? "true" : "false") + ",";
  json += "\"wifi_ready\":" + String(state.system.wifiReady ? "true" : "false");
  json += "}";
  json += "}";
  return json;
}

String LiveUpload::jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char ch = value[index];
    switch (ch) {
      case '\\':
        escaped += "\\\\";
        break;
      case '"':
        escaped += "\\\"";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        escaped += ch;
        break;
    }
  }
  return escaped;
}
