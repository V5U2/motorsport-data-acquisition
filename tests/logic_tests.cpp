#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

#include "Logic.h"

namespace {

int failures = 0;

void expectNear(const float actual, const float expected, const float tolerance, const std::string &message) {
  if (std::fabs(actual - expected) > tolerance) {
    std::cerr << "FAIL: " << message << " expected=" << expected << " actual=" << actual << "\n";
    ++failures;
  }
}

void expectEqual(const std::string &actual, const std::string &expected, const std::string &message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " expected='" << expected << "' actual='" << actual << "'\n";
    ++failures;
  }
}

void expectFault(const SensorFault actual, const SensorFault expected, const std::string &message) {
  if (actual != expected) {
    std::cerr << "FAIL: " << message << " expected=" << sensorFaultToString(expected)
              << " actual=" << sensorFaultToString(actual) << "\n";
    ++failures;
  }
}

void testCurrentConversion() {
  expectNear(Logic::currentFromVoltage(0.66f, 165.0f), 4.0f, 0.001f, "4 mA conversion");
  expectNear(Logic::currentFromVoltage(3.30f, 165.0f), 20.0f, 0.001f, "20 mA conversion");
  expectNear(Logic::currentFromVoltage(1.98f, 165.0f), 12.0f, 0.001f, "12 mA conversion");
  expectNear(Logic::currentFromVoltage(1.0f, 0.0f), 0.0f, 0.001f, "zero-ohm guard");
}

void testFaultThresholds() {
  expectFault(Logic::determineSensorFault(1.0f, 12.0f, false), SensorFault::AdcUnavailable,
              "ADC unavailable fault");
  expectFault(Logic::determineSensorFault(3.28f, 19.9f, true), SensorFault::AdcSaturation,
              "saturation threshold");
  expectFault(Logic::determineSensorFault(0.59f, 3.59f, true), SensorFault::Underrange,
              "underrange below threshold");
  expectFault(Logic::determineSensorFault(0.594f, 3.60f, true), SensorFault::None,
              "3.6 mA is valid");
  expectFault(Logic::determineSensorFault(3.20f, 20.81f, true), SensorFault::Overrange,
              "overrange above threshold");
  expectFault(Logic::determineSensorFault(3.20f, 20.80f, true), SensorFault::None,
              "20.8 mA is valid");
}

void testEngineeringScaling() {
  expectNear(Logic::scaleEngineeringValue(4.0f, 4.0f, 20.0f, 0.0f, 10.0f), 0.0f, 0.001f,
             "4 mA maps to min engineering value");
  expectNear(Logic::scaleEngineeringValue(12.0f, 4.0f, 20.0f, 0.0f, 10.0f), 5.0f, 0.001f,
             "midpoint maps correctly");
  expectNear(Logic::scaleEngineeringValue(20.0f, 4.0f, 20.0f, 0.0f, 10.0f), 10.0f, 0.001f,
             "20 mA maps to max engineering value");
  expectNear(Logic::scaleEngineeringValue(2.0f, 4.0f, 20.0f, 0.0f, 10.0f), 0.0f, 0.001f,
             "below range clamps low");
  expectNear(Logic::scaleEngineeringValue(25.0f, 4.0f, 20.0f, 0.0f, 10.0f), 10.0f, 0.001f,
             "above range clamps high");
  expectNear(Logic::scaleEngineeringValue(12.0f, 4.0f, 4.0f, 0.0f, 10.0f), 0.0f, 0.001f,
             "invalid scaling range guard");
}

void testFilter() {
  expectNear(Logic::applyLowPassFilter(10.0f, 20.0f, 0.18f), 11.8f, 0.001f, "normal low-pass step");
  expectNear(Logic::applyLowPassFilter(10.0f, 20.0f, -1.0f), 10.0f, 0.001f, "alpha clamps low");
  expectNear(Logic::applyLowPassFilter(10.0f, 20.0f, 2.0f), 20.0f, 0.001f, "alpha clamps high");
}

void testTimestampFormatting() {
  expectEqual(Logic::fallbackTimestamp(12345), "boot+12345", "fallback timestamp");
  expectEqual(Logic::formatTimestamp(2026, 3, 4, 5, 6, 7), "2026-03-04 05:06:07",
              "RTC timestamp formatting");
  expectEqual(Logic::formatDateStamp(2026, 3, 4), "20260304", "date stamp formatting");
}

void testFileNameNormalization() {
  expectEqual(Logic::normalizeLogFileName(" logs-20260314.csv "), "/logs-20260314.csv",
              "trims and prepends slash");
  expectEqual(Logic::normalizeLogFileName("/nested/logs-20260314.csv"), "/nested/logs-20260314.csv",
              "keeps valid nested path");
  expectEqual(Logic::normalizeLogFileName("../secret.csv"), "", "rejects traversal");
  expectEqual(Logic::normalizeLogFileName("bad name.csv"), "", "rejects spaces");
  expectEqual(Logic::normalizeLogFileName("bad$name.csv"), "", "rejects symbols");
  expectEqual(Logic::normalizeLogFileName(""), "", "rejects empty names");
}

}  // namespace

int main() {
  testCurrentConversion();
  testFaultThresholds();
  testEngineeringScaling();
  testFilter();
  testTimestampFormatting();
  testFileNameNormalization();

  if (failures == 0) {
    std::cout << "All host logic tests passed\n";
    return EXIT_SUCCESS;
  }

  std::cerr << failures << " host logic test(s) failed\n";
  return EXIT_FAILURE;
}
