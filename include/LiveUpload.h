#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "AppConfig.h"
#include "Types.h"

class LiveUpload {
 public:
  LiveUpload();

  bool begin(const AppConfig::UploadConfig &config, bool enabled);
  void loop();
  bool publishIfDue(const AppState &state);

  bool isEnabled() const;
  bool isConnected();
  String protocolName() const;
  String sessionId() const;
  String lastError() const;
  uint32_t lastSequence() const;

 private:
  bool reconnect(uint32_t nowMs);
  void publishOfflineStatusAndDisconnect();
  bool publishStatus(bool connected);
  bool publishSnapshot(const AppState &state);
  String liveTopic() const;
  String statusTopic() const;
  String buildStatusJson(bool connected) const;
  String buildSnapshotJson(const AppState &state, uint32_t sequence) const;
  static String jsonEscape(const String &value);

  WiFiClient networkClient_;
  PubSubClient mqttClient_;
  AppConfig::UploadConfig config_{};
  bool enabled_ = false;
  uint32_t lastPublishMs_ = 0;
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t lastSequence_ = 0;
  String deviceId_;
  String sessionId_;
  String clientId_;
  String lastError_;
};
