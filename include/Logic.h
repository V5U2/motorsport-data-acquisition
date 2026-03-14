#pragma once

#include <stdint.h>

#include <string>
#include <string_view>

#include "SensorTypes.h"

namespace Logic {

float currentFromVoltage(float voltage, float shuntResistanceOhms);
SensorFault determineSensorFault(float voltage, float currentmA, bool adcAvailable);
float scaleEngineeringValue(float currentmA,
                            float currentMinmA,
                            float currentMaxmA,
                            float engMin,
                            float engMax);
float applyLowPassFilter(float previousValue, float currentValue, float alpha);

std::string fallbackTimestamp(uint32_t uptimeMs);
std::string formatTimestamp(int year, int month, int day, int hour, int minute, int second);
std::string formatDateStamp(int year, int month, int day);
std::string normalizeLogFileName(std::string_view rawName);

}  // namespace Logic
