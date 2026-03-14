#pragma once

#include <Arduino.h>
#include "SensorTypes.h"

struct SensorSnapshot {
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
  bool rtcReady;
  bool sdReady;
  bool wifiReady;
  String wifiMode;
  String ipAddress;
  String currentLogFile;
  String lastLogError;
};

struct AppState {
  SensorSnapshot pressure;
  SensorSnapshot temperature;
  SystemStatus system;
  uint32_t uptimeMs;
  String timestamp;
};
