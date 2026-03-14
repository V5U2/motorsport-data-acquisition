#pragma once

#include <Arduino.h>
#include <FS.h>
#include <SD.h>
#include <SPI.h>
#include "AppConfig.h"
#include "Timekeeper.h"
#include "Types.h"

class CsvLogger {
 public:
  bool begin(uint8_t chipSelectPin, SPIClass &spi);
  bool logRow(const Timekeeper &timekeeper,
              uint32_t uptimeMs,
              const SensorSnapshot &pressure,
              const SensorSnapshot &temperature);
  void flushIfNeeded(uint32_t uptimeMs);
  bool isReady() const;
  String currentFileName() const;
  String lastError() const;
  String listFilesJson() const;
  File openReadOnly(const String &userVisibleName) const;

 private:
  bool ensureFileOpen(const String &dateStamp);
  String normalizeFileName(const String &userVisibleName) const;
  void writeHeaderIfNeeded();

  bool ready_ = false;
  String currentFileName_;
  String lastError_;
  File file_;
  uint32_t lastFlushMs_ = 0;
  uint16_t rowsSinceFlush_ = 0;
};
