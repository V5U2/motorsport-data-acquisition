#pragma once

#include <array>
#include <Arduino.h>
#include "AppConfig.h"
#include "SensorTypes.h"

struct SensorSnapshot {
  const char *id;
  const char *name;
  const char *units;
  float rawVoltage;
  float loopCurrentmA;
  float engineeringValue;
  float filteredValue;
  float minValue;
  float maxValue;
  float warnLow;
  float warnHigh;
  float engMin;
  float engMax;
  SensorFault activeFault;
  SensorFault latchedFault;
  bool hasValidSample;
};

struct SystemStatus {
  bool adcReady;
  bool displayEnabled;
  bool rtcEnabled;
  bool rtcReady;
  bool sdEnabled;
  bool sdReady;
  bool wifiReady;
  bool uploadEnabled;
  bool uploadConnected;
  String wifiMode;
  String ipAddress;
  String currentLogFile;
  String lastLogError;
  String uploadProtocol;
  String uploadSessionId;
  String lastUploadError;
  uint32_t lastUploadSequence;
};

struct AppState {
  std::array<SensorSnapshot, AppConfig::kSensorCount> sensors;
  SystemStatus system;
  uint32_t uptimeMs;
  String timestamp;
};
