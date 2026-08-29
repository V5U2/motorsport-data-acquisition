#include "RuntimeSettings.h"

#include <EEPROM.h>
#include <cstddef>
#include <cstring>

bool RuntimeSettings::begin(const AppConfig::UploadConfig &defaults,
                            const bool defaultUploadEnabled) {
  defaults_ = defaults;
  EEPROM.begin(sizeof(Record));

  Record record{};
  EEPROM.get(0, record);
  if (!valid(record)) {
    record.magic = kMagic;
    record.version = kVersion;
    record.size = sizeof(Record);
    record.uploadEnabled = defaultUploadEnabled ? 1 : 0;
    record.mqttPort = defaults.mqttPort;
    strncpy(record.mqttHost, defaults.mqttHost, sizeof(record.mqttHost) - 1);
    record.checksum = checksum(record);
  }

  apply(record);
  return true;
}

bool RuntimeSettings::saveUploadServer(const String &host,
                                       const uint16_t port,
                                       const bool enabled) {
  if (host.isEmpty() || host.length() >= kHostCapacity || port == 0) {
    return false;
  }

  Record record{};
  record.magic = kMagic;
  record.version = kVersion;
  record.size = sizeof(Record);
  record.uploadEnabled = enabled ? 1 : 0;
  record.mqttPort = port;
  host.toCharArray(record.mqttHost, sizeof(record.mqttHost));
  record.checksum = checksum(record);

  EEPROM.put(0, record);
  if (!EEPROM.commit()) {
    return false;
  }

  apply(record);
  return true;
}

const AppConfig::UploadConfig &RuntimeSettings::uploadConfig() const { return uploadConfig_; }

bool RuntimeSettings::liveUploadEnabled() const { return uploadEnabled_; }

String RuntimeSettings::uploadServerLabel() const {
  if (strlen(mqttHost_) == 0) {
    return "Not configured";
  }
  return String(mqttHost_) + ":" + String(uploadConfig_.mqttPort);
}

uint32_t RuntimeSettings::checksum(const Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < offsetof(Record, checksum); ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}

bool RuntimeSettings::valid(const Record &record) {
  return record.magic == kMagic && record.version == kVersion &&
         record.size == sizeof(Record) && record.mqttPort != 0 &&
         record.mqttHost[0] != '\0' &&
         record.mqttHost[kHostCapacity - 1] == '\0' &&
         record.checksum == checksum(record);
}

void RuntimeSettings::apply(const Record &record) {
  memset(mqttHost_, 0, sizeof(mqttHost_));
  strncpy(mqttHost_, record.mqttHost, sizeof(mqttHost_) - 1);
  uploadConfig_ = defaults_;
  uploadConfig_.mqttHost = mqttHost_;
  uploadConfig_.mqttPort = record.mqttPort;
  uploadEnabled_ = record.uploadEnabled != 0;
}
