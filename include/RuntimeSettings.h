#pragma once

#include <Arduino.h>

#include "AppConfig.h"

class RuntimeSettings {
 public:
  bool begin(const AppConfig::UploadConfig &defaults, bool defaultUploadEnabled);
  bool saveUploadServer(const String &host, uint16_t port, bool enabled);

  const AppConfig::UploadConfig &uploadConfig() const;
  bool liveUploadEnabled() const;
  String uploadServerLabel() const;

 private:
  static constexpr uint32_t kMagic = 0x4D444131UL;
  static constexpr uint16_t kVersion = 1;
  static constexpr size_t kHostCapacity = 64;

  struct Record {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint8_t uploadEnabled;
    uint8_t reserved;
    uint16_t mqttPort;
    char mqttHost[kHostCapacity];
    uint32_t checksum;
  };

  static uint32_t checksum(const Record &record);
  static bool valid(const Record &record);
  void apply(const Record &record);

  AppConfig::UploadConfig defaults_{};
  AppConfig::UploadConfig uploadConfig_{};
  char mqttHost_[kHostCapacity]{};
  bool uploadEnabled_ = false;
};
