#include "LiveUpload.h"

#include <ArduinoJson.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#include <esp_system.h>
#endif

#include "Logic.h"

namespace {

constexpr uint16_t kMqttBufferSize = 2048;
constexpr uint16_t kMqttSocketTimeoutSeconds = 1;
constexpr uint32_t kNetworkClientTimeoutMs = 500;
constexpr uint32_t kStatusHeartbeatMs = 30000;
constexpr uint32_t kPairingCodeRefreshMs = 10UL * 60UL * 1000UL;
constexpr size_t kRemoteConfigJsonCapacity = 1024;

bool validRemoteText(const char *value, const size_t maximumLength) {
  if (value == nullptr) {
    return true;
  }
  const size_t length = strlen(value);
  if (length == 0 || length > maximumLength) {
    return false;
  }
  for (size_t index = 0; index < length; ++index) {
    if (static_cast<uint8_t>(value[index]) < 0x20) {
      return false;
    }
  }
  return true;
}

}  // namespace

LiveUpload::LiveUpload() : mqttClient_(networkClient_) {}

bool LiveUpload::begin(const AppConfig::UploadConfig &config,
                       const bool enabled,
                       const bool remoteManagementEnabled,
                       const uint32_t appliedConfigVersion) {
  config_ = config;
  enabled_ = enabled;
  remoteManagementEnabled_ = remoteManagementEnabled;
  appliedConfigVersion_ = appliedConfigVersion;

  const std::string normalizedDevice = Logic::normalizeTopicSegment(config.deviceId);
  deviceId_ = normalizedDevice.empty() ? "mda-logger" : String(normalizedDevice.c_str());

#if defined(ESP8266)
  const uint32_t bootCounter = ESP.random();
  const uint32_t chipSuffix = ESP.getChipId() & 0xFFFFUL;
#else
  const uint32_t bootCounter = esp_random();
  const uint32_t chipSuffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFULL);
#endif
  sessionId_ = String(Logic::formatSessionId(deviceId_.c_str(), bootCounter).c_str());
  clientId_ = deviceId_ + "-" + String(chipSuffix, HEX);
  rotatePairingCode(millis());

  mqttClient_.setServer(config_.mqttHost, config_.mqttPort);
  mqttClient_.setBufferSize(kMqttBufferSize);
  mqttClient_.setSocketTimeout(kMqttSocketTimeoutSeconds);
  mqttClient_.setCallback(
      [this](char *topic, uint8_t *payload, unsigned int length) {
        handleMqttMessage(topic, payload, length);
      });
  networkClient_.setTimeout(kNetworkClientTimeoutMs);

  if (!enabled_) {
    lastError_ = "Live upload disabled";
    return false;
  }

  if (strlen(config_.mqttHost) == 0) {
    lastError_ = "MQTT host not configured";
    return false;
  }

  if (!Logic::mqttIdentityMatches(config_.deviceId, config_.mqttUsername)) {
    lastError_ = "MQTT username must match normalized device ID " + deviceId_;
    return false;
  }

  lastError_ = "";
  return true;
}

void LiveUpload::loop() {
  if (!enabled_) {
    return;
  }

  const uint32_t nowMs = millis();
  if (remoteManagementEnabled_ &&
      Logic::intervalElapsed(nowMs, pairingCodeGeneratedMs_, kPairingCodeRefreshMs)) {
    rotatePairingCode(nowMs);
    if (mqttClient_.connected()) {
      publishStatus(true);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    publishOfflineStatusAndDisconnect();
    if (lastError_.isEmpty()) {
      lastError_ = "Wi-Fi disconnected";
    }
    return;
  }

  mqttClient_.loop();
  if (mqttClient_.connected() && (nowMs - lastStatusPublishMs_) >= kStatusHeartbeatMs) {
    publishStatus(true);
  }
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

String LiveUpload::serverName() const {
  if (strlen(config_.mqttHost) == 0) {
    return "Not configured";
  }
  return String(config_.mqttHost) + ":" + String(config_.mqttPort);
}

String LiveUpload::sessionId() const { return sessionId_; }

String LiveUpload::lastError() const { return lastError_; }

uint32_t LiveUpload::lastSequence() const { return lastSequence_; }

String LiveUpload::pairingCode() const {
  return remoteManagementEnabled_ ? pairingCode_ : "";
}

uint32_t LiveUpload::pairingCodeExpiresInSeconds() const {
  if (!remoteManagementEnabled_ || pairingCode_.isEmpty()) {
    return 0;
  }
  const uint32_t elapsedMs = static_cast<uint32_t>(millis() - pairingCodeGeneratedMs_);
  if (elapsedMs >= kPairingCodeRefreshMs) {
    return 0;
  }
  return (kPairingCodeRefreshMs - elapsedMs + 999UL) / 1000UL;
}

void LiveUpload::rotatePairingCode(const uint32_t nowMs) {
#if defined(ESP8266)
  const uint64_t entropy = (static_cast<uint64_t>(ESP.random()) << 32) | ESP.random();
#else
  const uint64_t entropy = (static_cast<uint64_t>(esp_random()) << 32) | esp_random();
#endif
  pairingCode_ = Logic::formatPairingCode(entropy).c_str();
  pairingCodeGeneratedMs_ = nowMs;
}

bool LiveUpload::consumeRemoteConfig(RemoteConfig &config) {
  if (!hasPendingRemoteConfig_) {
    return false;
  }
  config = pendingRemoteConfig_;
  hasPendingRemoteConfig_ = false;
  return true;
}

void LiveUpload::acknowledgeRemoteConfig(const uint32_t version) {
  appliedConfigVersion_ = version;
  managementStatus_ = "applied";
  managementError_ = "";
  if (mqttClient_.connected()) {
    publishStatus(true);
  }
}

bool LiveUpload::reconnect(const uint32_t nowMs) {
  if (strlen(config_.mqttHost) == 0) {
    lastError_ = "MQTT host not configured";
    return false;
  }

  if ((nowMs - lastReconnectAttemptMs_) < config_.reconnectIntervalMs) {
    return false;
  }

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

  // Start the backoff after the blocking client call completes. Otherwise an
  // attempt lasting longer than the configured interval is retried immediately
  // and starves sensor sampling, OTA, and the web server.
  lastReconnectAttemptMs_ = millis();

  if (!connected) {
    lastError_ = "MQTT connect failed rc=" + String(mqttClient_.state());
    return false;
  }

  if (!publishStatus(true)) {
    return false;
  }
  if (remoteManagementEnabled_ && !mqttClient_.subscribe(desiredConfigTopic().c_str(), 1)) {
    lastError_ = "MQTT desired config subscribe failed";
    mqttClient_.disconnect();
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
  lastStatusPublishMs_ = millis();
  return true;
}

void LiveUpload::handleMqttMessage(char *topic, uint8_t *payload, unsigned int length) {
  if (!remoteManagementEnabled_ || String(topic) != desiredConfigTopic()) {
    return;
  }
  RemoteConfig candidate{};
  if (!parseRemoteConfig(payload, length, candidate)) {
    managementStatus_ = "rejected";
    managementError_ = "Invalid desired configuration";
    return;
  }
  if (candidate.version <= appliedConfigVersion_) {
    return;
  }
  pendingRemoteConfig_ = candidate;
  hasPendingRemoteConfig_ = true;
  managementStatus_ = "pending";
  managementError_ = "";
}

bool LiveUpload::parseRemoteConfig(const uint8_t *payload,
                                   const unsigned int length,
                                   RemoteConfig &config) {
  if (length == 0 || length > kRemoteConfigJsonCapacity) {
    return false;
  }
  StaticJsonDocument<kRemoteConfigJsonCapacity> document;
  if (deserializeJson(document, payload, length) != DeserializationError::Ok) {
    return false;
  }
  if (document["schema_version"].as<int>() != 1 ||
      String(document["device_id"].as<const char *>()) != deviceId_) {
    return false;
  }
  const uint32_t version = document["config_version"].as<uint32_t>();
  JsonObject settings = document["settings"].as<JsonObject>();
  if (version == 0 || settings.isNull()) {
    return false;
  }
  for (JsonPair item : settings) {
    const String key = item.key().c_str();
    if (key != "upload_enabled" && key != "ntp_primary" && key != "ntp_secondary" &&
        key != "tz_rule" && key != "tz_label") {
      return false;
    }
  }
  config = {};
  config.version = version;
  if (settings.containsKey("upload_enabled")) {
    if (!settings["upload_enabled"].is<bool>()) {
      return false;
    }
    config.hasUploadEnabled = true;
    config.uploadEnabled = settings["upload_enabled"].as<bool>();
  }
  const char *primary = settings["ntp_primary"] | nullptr;
  const char *secondary = settings["ntp_secondary"] | nullptr;
  const char *rule = settings["tz_rule"] | nullptr;
  const char *label = settings["tz_label"] | nullptr;
  if (!validRemoteText(primary, 63) || !validRemoteText(secondary, 63) ||
      !validRemoteText(rule, 63) || !validRemoteText(label, 31)) {
    return false;
  }
  config.ntpPrimary = primary == nullptr ? "" : primary;
  config.ntpSecondary = secondary == nullptr ? "" : secondary;
  config.timeZoneRule = rule == nullptr ? "" : rule;
  config.timeZoneLabel = label == nullptr ? "" : label;
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

String LiveUpload::desiredConfigTopic() const {
  String prefix = config_.topicPrefix;
  while (prefix.startsWith("/")) {
    prefix.remove(0, 1);
  }
  while (prefix.endsWith("/")) {
    prefix.remove(prefix.length() - 1);
  }
  return prefix + "/" + deviceId_ + "/config/desired";
}

String LiveUpload::buildStatusJson(const bool connected) const {
  String json = "{";
  json += "\"schema_version\":" + String(Logic::kLivePayloadSchemaVersion) + ",";
  json += "\"device_id\":\"" + jsonEscape(deviceId_) + "\",";
  json += "\"session_id\":\"" + jsonEscape(sessionId_) + "\",";
  json += "\"protocol\":\"mqtt\",";
  json += "\"connected\":" + String(connected ? "true" : "false");
  if (remoteManagementEnabled_) {
    json += ",\"management\":{";
    json += "\"enabled\":true,";
    json += "\"pairing_code\":\"" + jsonEscape(pairingCode_) + "\",";
    json += "\"pairing_expires_in_s\":" + String(pairingCodeExpiresInSeconds()) + ",";
    json += "\"config_version\":" + String(appliedConfigVersion_) + ",";
    json += "\"status\":\"" + jsonEscape(managementStatus_) + "\",";
    json += "\"error\":\"" + jsonEscape(managementError_) + "\",";
    json += "\"firmware_version\":\"" + jsonEscape(APEXI_FIRMWARE_VERSION) + "\"";
    json += "}";
  }
  json += "}";
  return json;
}

String LiveUpload::buildSnapshotJson(const AppState &state, const uint32_t sequence) const {
  String json = "{";
  json += "\"schema_version\":" + String(Logic::kLivePayloadSchemaVersion) + ",";
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
