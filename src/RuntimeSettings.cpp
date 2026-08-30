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
    LegacyRecord legacy{};
    EEPROM.get(0, legacy);
    populateDefaults(record, defaults, defaultUploadEnabled);
    if (valid(legacy)) {
      record.uploadEnabled = legacy.uploadEnabled;
      record.mqttPort = legacy.mqttPort;
      strncpy(record.mqttHost, legacy.mqttHost, sizeof(record.mqttHost) - 1);
    }
    record.checksum = checksum(record);
    EEPROM.put(0, record);
    EEPROM.commit();
  }

  apply(record);
  return true;
}

bool RuntimeSettings::save(const String &host,
                           const uint16_t port,
                           const bool enabled,
                           const String &ntpPrimary,
                           const String &ntpSecondary,
                           const String &timeZoneRule,
                           const String &timeZoneLabel) {
  if (!validText(host, kHostCapacity) || port == 0 ||
      !validText(ntpPrimary, kNtpCapacity) ||
      !validText(ntpSecondary, kNtpCapacity) ||
      !validText(timeZoneRule, kTimeZoneRuleCapacity) ||
      !validText(timeZoneLabel, kTimeZoneLabelCapacity)) {
    return false;
  }

  Record record{};
  record.magic = kMagic;
  record.version = kVersion;
  record.size = sizeof(Record);
  record.uploadEnabled = enabled ? 1 : 0;
  record.mqttPort = port;
  host.toCharArray(record.mqttHost, sizeof(record.mqttHost));
  ntpPrimary.toCharArray(record.ntpPrimary, sizeof(record.ntpPrimary));
  ntpSecondary.toCharArray(record.ntpSecondary, sizeof(record.ntpSecondary));
  timeZoneRule.toCharArray(record.timeZoneRule, sizeof(record.timeZoneRule));
  timeZoneLabel.toCharArray(record.timeZoneLabel, sizeof(record.timeZoneLabel));
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

const char *RuntimeSettings::ntpPrimary() const { return ntpPrimary_; }

const char *RuntimeSettings::ntpSecondary() const { return ntpSecondary_; }

const char *RuntimeSettings::timeZoneRule() const { return timeZoneRule_; }

const char *RuntimeSettings::timeZoneLabel() const { return timeZoneLabel_; }

uint32_t RuntimeSettings::checksum(const Record &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < offsetof(Record, checksum); ++index) {
    value ^= bytes[index];
    value *= 16777619UL;
  }
  return value;
}

uint32_t RuntimeSettings::checksum(const LegacyRecord &record) {
  const uint8_t *bytes = reinterpret_cast<const uint8_t *>(&record);
  uint32_t value = 2166136261UL;
  for (size_t index = 0; index < offsetof(LegacyRecord, checksum); ++index) {
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
         record.ntpPrimary[0] != '\0' &&
         record.ntpPrimary[kNtpCapacity - 1] == '\0' &&
         record.ntpSecondary[0] != '\0' &&
         record.ntpSecondary[kNtpCapacity - 1] == '\0' &&
         record.timeZoneRule[0] != '\0' &&
         record.timeZoneRule[kTimeZoneRuleCapacity - 1] == '\0' &&
         record.timeZoneLabel[0] != '\0' &&
         record.timeZoneLabel[kTimeZoneLabelCapacity - 1] == '\0' &&
         record.checksum == checksum(record);
}

bool RuntimeSettings::valid(const LegacyRecord &record) {
  return record.magic == kMagic && record.version == 1 &&
         record.size == sizeof(LegacyRecord) && record.mqttPort != 0 &&
         record.mqttHost[0] != '\0' &&
         record.mqttHost[kHostCapacity - 1] == '\0' &&
         record.checksum == checksum(record);
}

bool RuntimeSettings::validText(const String &value, const size_t capacity) {
  if (value.isEmpty() || value.length() >= capacity) {
    return false;
  }
  for (size_t index = 0; index < value.length(); ++index) {
    const uint8_t character = static_cast<uint8_t>(value[index]);
    if (character < 0x20 || character == 0x7F) {
      return false;
    }
  }
  return true;
}

void RuntimeSettings::populateDefaults(Record &record,
                                       const AppConfig::UploadConfig &defaults,
                                       const bool defaultUploadEnabled) {
  record = {};
  record.magic = kMagic;
  record.version = kVersion;
  record.size = sizeof(Record);
  record.uploadEnabled = defaultUploadEnabled ? 1 : 0;
  record.mqttPort = defaults.mqttPort;
  strncpy(record.mqttHost, defaults.mqttHost, sizeof(record.mqttHost) - 1);
  strncpy(record.ntpPrimary, "pool.ntp.org", sizeof(record.ntpPrimary) - 1);
  strncpy(record.ntpSecondary, "time.google.com", sizeof(record.ntpSecondary) - 1);
  strncpy(record.timeZoneRule, "AWST-8", sizeof(record.timeZoneRule) - 1);
  strncpy(record.timeZoneLabel, "AWST", sizeof(record.timeZoneLabel) - 1);
}

void RuntimeSettings::apply(const Record &record) {
  memset(mqttHost_, 0, sizeof(mqttHost_));
  memset(ntpPrimary_, 0, sizeof(ntpPrimary_));
  memset(ntpSecondary_, 0, sizeof(ntpSecondary_));
  memset(timeZoneRule_, 0, sizeof(timeZoneRule_));
  memset(timeZoneLabel_, 0, sizeof(timeZoneLabel_));
  strncpy(mqttHost_, record.mqttHost, sizeof(mqttHost_) - 1);
  strncpy(ntpPrimary_, record.ntpPrimary, sizeof(ntpPrimary_) - 1);
  strncpy(ntpSecondary_, record.ntpSecondary, sizeof(ntpSecondary_) - 1);
  strncpy(timeZoneRule_, record.timeZoneRule, sizeof(timeZoneRule_) - 1);
  strncpy(timeZoneLabel_, record.timeZoneLabel, sizeof(timeZoneLabel_) - 1);
  uploadConfig_ = defaults_;
  uploadConfig_.mqttHost = mqttHost_;
  uploadConfig_.mqttPort = record.mqttPort;
  uploadEnabled_ = record.uploadEnabled != 0;
}
