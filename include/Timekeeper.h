#pragma once

#include <Arduino.h>
#include <RTClib.h>

class Timekeeper {
 public:
  bool begin(TwoWire &wire);
  bool isReady() const;
  String logTimestamp(uint32_t uptimeMs);
  String dateStamp();
  String lastError() const;

 private:
  RTC_DS3231 rtc_;
  bool ready_ = false;
  String lastError_;
};
