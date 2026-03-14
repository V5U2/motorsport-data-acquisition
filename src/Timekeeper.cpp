#include "Timekeeper.h"

#include "Logic.h"

bool Timekeeper::begin(TwoWire &wire) {
  if (!rtc_.begin(&wire)) {
    ready_ = false;
    lastError_ = "DS3231 not detected";
    return false;
  }

  if (rtc_.lostPower()) {
    ready_ = false;
    lastError_ = "RTC lost power or time invalid";
    return false;
  }

  const DateTime now = rtc_.now();
  if (now.year() < 2024) {
    ready_ = false;
    lastError_ = "RTC time out of range";
    return false;
  }

  ready_ = true;
  lastError_ = "";
  return true;
}

bool Timekeeper::isReady() const { return ready_; }

String Timekeeper::logTimestamp(const uint32_t uptimeMs) const {
  if (!ready_) {
    return String(Logic::fallbackTimestamp(uptimeMs).c_str());
  }

  const DateTime now = rtc_.now();
  return String(Logic::formatTimestamp(now.year(),
                                       now.month(),
                                       now.day(),
                                       now.hour(),
                                       now.minute(),
                                       now.second())
                    .c_str());
}

String Timekeeper::dateStamp() const {
  if (!ready_) {
    return "boot";
  }

  const DateTime now = rtc_.now();
  return String(Logic::formatDateStamp(now.year(), now.month(), now.day()).c_str());
}

String Timekeeper::lastError() const { return lastError_; }
