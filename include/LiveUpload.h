#pragma once

#include <Arduino.h>
#include <PubSubClient.h>
#include <WiFiClient.h>

#include "AppConfig.h"
#include "RemoteConfig.h"
#include "Types.h"

class LiveUpload {
 public:
  LiveUpload();

  bool begin(const AppConfig::UploadConfig &config,
             bool enabled,
             bool remoteManagementEnabled,
             uint32_t appliedConfigVersion);
  void loop();
  bool publishIfDue(const AppState &state);

  bool isEnabled() const;
  bool isConnected();
  String protocolName() const;
  String serverName() const;
  String sessionId() const;
  String lastError() const;
  uint32_t lastSequence() const;
  String pairingCode() const;
  uint32_t pairingCodeExpiresInSeconds() const;
  bool consumeRemoteConfig(RemoteConfig &config);
  void acknowledgeRemoteConfig(uint32_t version);

 private:
  bool reconnect(uint32_t nowMs);
  void publishOfflineStatusAndDisconnect();
  bool publishStatus(bool connected);
  bool publishSnapshot(const AppState &state);
  void handleMqttMessage(char *topic, uint8_t *payload, unsigned int length);
  bool parseRemoteConfig(const uint8_t *payload, unsigned int length, RemoteConfig &config);
  void rotatePairingCode(uint32_t nowMs);
  String liveTopic() const;
  String statusTopic() const;
  String desiredConfigTopic() const;
  String buildStatusJson(bool connected) const;
  String buildSnapshotJson(const AppState &state, uint32_t sequence) const;
  static String jsonEscape(const String &value);

  WiFiClient networkClient_;
  PubSubClient mqttClient_;
  AppConfig::UploadConfig config_{};
  bool enabled_ = false;
  bool remoteManagementEnabled_ = false;
  uint32_t lastPublishMs_ = 0;
  uint32_t lastReconnectAttemptMs_ = 0;
  uint32_t lastSequence_ = 0;
  uint32_t lastStatusPublishMs_ = 0;
  uint32_t appliedConfigVersion_ = 0;
  String deviceId_;
  String sessionId_;
  String clientId_;
  String lastError_;
  String pairingCode_;
  uint32_t pairingCodeGeneratedMs_ = 0;
  String managementStatus_ = "ready";
  String managementError_;
  RemoteConfig pendingRemoteConfig_{};
  bool hasPendingRemoteConfig_ = false;
};
