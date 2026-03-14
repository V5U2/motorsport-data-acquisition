#include "Logic.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace {

std::string trimWhitespace(std::string_view input) {
  size_t start = 0;
  while (start < input.size() && std::isspace(static_cast<unsigned char>(input[start])) != 0) {
    ++start;
  }

  size_t end = input.size();
  while (end > start && std::isspace(static_cast<unsigned char>(input[end - 1])) != 0) {
    --end;
  }

  return std::string(input.substr(start, end - start));
}

}  // namespace

namespace Logic {

float currentFromVoltage(const float voltage, const float shuntResistanceOhms) {
  if (shuntResistanceOhms <= 0.0f) {
    return 0.0f;
  }
  return (voltage / shuntResistanceOhms) * 1000.0f;
}

SensorFault determineSensorFault(const float voltage,
                                 const float currentmA,
                                 const bool adcAvailable) {
  if (!adcAvailable) {
    return SensorFault::AdcUnavailable;
  }
  if (voltage >= 3.28f) {
    return SensorFault::AdcSaturation;
  }
  if (currentmA < 3.6f) {
    return SensorFault::Underrange;
  }
  if (currentmA > 20.8f) {
    return SensorFault::Overrange;
  }
  return SensorFault::None;
}

float scaleEngineeringValue(const float currentmA,
                            const float currentMinmA,
                            const float currentMaxmA,
                            const float engMin,
                            const float engMax) {
  if (currentMaxmA <= currentMinmA) {
    return engMin;
  }

  const float normalized = (currentmA - currentMinmA) / (currentMaxmA - currentMinmA);
  const float clamped = std::clamp(normalized, 0.0f, 1.0f);
  return engMin + (clamped * (engMax - engMin));
}

float applyLowPassFilter(const float previousValue, const float currentValue, const float alpha) {
  const float clampedAlpha = std::clamp(alpha, 0.0f, 1.0f);
  return (clampedAlpha * currentValue) + ((1.0f - clampedAlpha) * previousValue);
}

std::string fallbackTimestamp(const uint32_t uptimeMs) {
  return "boot+" + std::to_string(uptimeMs);
}

std::string formatTimestamp(const int year,
                            const int month,
                            const int day,
                            const int hour,
                            const int minute,
                            const int second) {
  char buffer[24];
  std::snprintf(buffer,
                sizeof(buffer),
                "%04d-%02d-%02d %02d:%02d:%02d",
                year,
                month,
                day,
                hour,
                minute,
                second);
  return std::string(buffer);
}

std::string formatDateStamp(const int year, const int month, const int day) {
  char buffer[16];
  std::snprintf(buffer, sizeof(buffer), "%04d%02d%02d", year, month, day);
  return std::string(buffer);
}

std::string normalizeLogFileName(std::string_view rawName) {
  std::string candidate = trimWhitespace(rawName);
  if (candidate.empty()) {
    return "";
  }

  if (candidate.front() != '/') {
    candidate.insert(candidate.begin(), '/');
  }

  for (const char ch : candidate) {
    const bool allowed = std::isalnum(static_cast<unsigned char>(ch)) != 0 ||
                         ch == '/' || ch == '-' || ch == '_' || ch == '.';
    if (!allowed) {
      return "";
    }
  }

  if (candidate.find("..") != std::string::npos) {
    return "";
  }

  return candidate;
}

}  // namespace Logic
