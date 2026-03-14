#include "CsvLogger.h"

#include "Logic.h"

bool CsvLogger::begin(const uint8_t chipSelectPin, SPIClass &spi) {
  ready_ = SD.begin(chipSelectPin, spi, 25000000);
  if (!ready_) {
    lastError_ = "microSD mount failed";
    return false;
  }
  lastError_ = "";
  return true;
}

bool CsvLogger::logRow(const Timekeeper &timekeeper,
                       const uint32_t uptimeMs,
                       const SensorSnapshot &pressure,
                       const SensorSnapshot &temperature) {
  if (!ready_) {
    return false;
  }

  if (!ensureFileOpen(timekeeper.dateStamp())) {
    return false;
  }

  const String row = timekeeper.logTimestamp(uptimeMs) + "," + String(uptimeMs) + "," +
                     String(pressure.filteredValue, 3) + "," + String(pressure.loopCurrentmA, 3) +
                     "," + sensorFaultToString(pressure.activeFault) + "," +
                     String(temperature.filteredValue, 3) + "," +
                     String(temperature.loopCurrentmA, 3) + "," +
                     sensorFaultToString(temperature.activeFault) + "\n";

  if (file_.print(row) == 0) {
    lastError_ = "CSV write failed";
    return false;
  }

  rowsSinceFlush_++;
  lastError_ = "";
  return true;
}

void CsvLogger::flushIfNeeded(const uint32_t uptimeMs) {
  if (!ready_ || !file_) {
    return;
  }

  if ((uptimeMs - lastFlushMs_) >= AppConfig::kTiming.loggerFlushIntervalMs ||
      rowsSinceFlush_ >= AppConfig::kTiming.loggerFlushRows) {
    file_.flush();
    lastFlushMs_ = uptimeMs;
    rowsSinceFlush_ = 0;
  }
}

bool CsvLogger::isReady() const { return ready_; }

String CsvLogger::currentFileName() const { return currentFileName_; }

String CsvLogger::lastError() const { return lastError_; }

String CsvLogger::listFilesJson() const {
  if (!ready_) {
    return "[]";
  }

  File root = SD.open("/");
  if (!root) {
    return "[]";
  }

  String json = "[";
  bool first = true;
  File entry = root.openNextFile();
  while (entry) {
    if (!entry.isDirectory()) {
      const String name = entry.name();
      if (name.endsWith(".csv")) {
        if (!first) {
          json += ",";
        }
        json += "{\"name\":\"" + name + "\",\"size\":" + String(entry.size()) + "}";
        first = false;
      }
    }
    entry.close();
    entry = root.openNextFile();
  }

  json += "]";
  return json;
}

File CsvLogger::openReadOnly(const String &userVisibleName) const {
  if (!ready_) {
    return File();
  }
  const String normalized = normalizeFileName(userVisibleName);
  if (normalized.isEmpty()) {
    return File();
  }
  return SD.open(normalized, FILE_READ);
}

bool CsvLogger::ensureFileOpen(const String &dateStamp) {
  const String desiredName = "/logs-" + dateStamp + ".csv";
  if (file_ && currentFileName_ == desiredName) {
    return true;
  }

  if (file_) {
    file_.flush();
    file_.close();
  }

  const bool newFile = !SD.exists(desiredName);
  file_ = SD.open(desiredName, FILE_APPEND);
  if (!file_) {
    lastError_ = "Open log file failed";
    return false;
  }

  currentFileName_ = desiredName;
  if (newFile || file_.size() == 0) {
    writeHeaderIfNeeded();
  }
  return true;
}

String CsvLogger::normalizeFileName(const String &userVisibleName) const {
  return String(Logic::normalizeLogFileName(userVisibleName.c_str()).c_str());
}

void CsvLogger::writeHeaderIfNeeded() {
  if (!file_) {
    return;
  }
  file_.println(
      "timestamp,uptime_ms,oil_pressure_value,oil_pressure_mA,oil_pressure_fault,oil_temp_value,"
      "oil_temp_mA,oil_temp_fault");
  file_.flush();
  lastFlushMs_ = 0;
  rowsSinceFlush_ = 0;
}
