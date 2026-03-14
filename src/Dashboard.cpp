#include "Dashboard.h"

#include <math.h>

namespace {

constexpr float kGaugeStartDeg = 150.0f;
constexpr float kGaugeSweepDeg = 240.0f;

float mapRange(const float value,
               const float inMin,
               const float inMax,
               const float outMin,
               const float outMax) {
  if (fabs(inMax - inMin) < 0.0001f) {
    return outMin;
  }
  const float ratio = (value - inMin) / (inMax - inMin);
  return outMin + (ratio * (outMax - outMin));
}

}  // namespace

bool Dashboard::begin() {
  tft_.init();
  tft_.setRotation(AppConfig::kDisplay.rotation);
  tft_.fillScreen(AppConfig::kDisplay.backgroundColor);
  tft_.setTextDatum(MC_DATUM);

  if (AppConfig::kPins.tftBacklight >= 0) {
    pinMode(AppConfig::kPins.tftBacklight, OUTPUT);
    digitalWrite(AppConfig::kPins.tftBacklight, HIGH);
  }

  ready_ = true;
  return true;
}

void Dashboard::render(const AppState &state) {
  if (!ready_) {
    return;
  }

  tft_.fillScreen(AppConfig::kDisplay.backgroundColor);
  if (screen_ == Screen::Main) {
    drawMainScreen(state);
  } else {
    drawDiagnosticsScreen(state);
  }
}

void Dashboard::nextScreen() {
  screen_ = (screen_ == Screen::Main) ? Screen::Diagnostics : Screen::Main;
}

Dashboard::Screen Dashboard::currentScreen() const { return screen_; }

void Dashboard::drawMainScreen(const AppState &state) {
  tft_.setTextColor(TFT_WHITE, AppConfig::kDisplay.backgroundColor);
  tft_.drawString("Motorsport Data Logger", 240, 20, 4);
  tft_.drawString(state.timestamp, 240, 46, 2);

  drawGauge(120, 185, 95, state.pressure);
  drawGauge(360, 185, 95, state.temperature);

  const bool faultLatched =
      state.pressure.latchedFault != SensorFault::None ||
      state.temperature.latchedFault != SensorFault::None;
  if (faultLatched) {
    tft_.fillRoundRect(55, 280, 370, 28, 6, TFT_RED);
    const String message = "Latched fault: " +
                           String(sensorFaultToString(state.pressure.latchedFault)) + " / " +
                           sensorFaultToString(state.temperature.latchedFault);
    tft_.setTextColor(TFT_WHITE, TFT_RED);
    tft_.drawString(message, 240, 294, 2);
  } else {
    tft_.fillRoundRect(55, 280, 370, 28, 6, TFT_DARKGREEN);
    tft_.setTextColor(TFT_WHITE, TFT_DARKGREEN);
    tft_.drawString("No latched sensor faults", 240, 294, 2);
  }
}

void Dashboard::drawDiagnosticsScreen(const AppState &state) {
  tft_.setTextColor(TFT_WHITE, AppConfig::kDisplay.backgroundColor);
  tft_.drawString("Diagnostics", 240, 18, 4);

  drawStatusLine(20, 52, "Log file", state.system.currentLogFile, TFT_CYAN);
  drawStatusLine(20, 76, "Wi-Fi", state.system.wifiMode + " " + state.system.ipAddress, TFT_GREEN);
  drawStatusLine(20, 100, "RTC", state.system.rtcReady ? "OK" : "FAULT", state.system.rtcReady ? TFT_GREEN : TFT_RED);
  drawStatusLine(20, 124, "SD", state.system.sdReady ? "OK" : state.system.lastLogError, state.system.sdReady ? TFT_GREEN : TFT_RED);
  drawStatusLine(20, 148, "ADC", state.system.adcReady ? "OK" : "FAULT", state.system.adcReady ? TFT_GREEN : TFT_RED);

  tft_.drawFastHLine(20, 170, 440, TFT_DARKGREY);

  const SensorSnapshot sensors[2] = {state.pressure, state.temperature};
  for (uint8_t index = 0; index < 2; ++index) {
    const int16_t top = 180 + (index * 62);
    const SensorSnapshot &sensor = sensors[index];
    tft_.setTextDatum(TL_DATUM);
    tft_.setTextColor(TFT_WHITE, AppConfig::kDisplay.backgroundColor);
    tft_.drawString(sensor.name, 24, top, 4);
    tft_.drawString("Value", 24, top + 24, 2);
    tft_.drawString(String(sensor.filteredValue, 2) + " " + sensor.units, 92, top + 24, 2);
    tft_.drawString("Loop", 220, top + 24, 2);
    tft_.drawString(String(sensor.loopCurrentmA, 2) + " mA", 278, top + 24, 2);
    tft_.drawString("Input", 24, top + 42, 2);
    tft_.drawString(String(sensor.rawVoltage, 3) + " V", 92, top + 42, 2);
    tft_.drawString("Fault", 220, top + 42, 2);
    tft_.setTextColor(valueColor(sensor), AppConfig::kDisplay.backgroundColor);
    tft_.drawString(sensorFaultToString(sensor.activeFault), 278, top + 42, 2);
  }
  tft_.setTextDatum(MC_DATUM);
}

void Dashboard::drawGauge(const int16_t centerX,
                          const int16_t centerY,
                          const int16_t radius,
                          const SensorSnapshot &sensor) {
  const float warnLowDeg = mapRange(sensor.warnLow, sensor.engMin, sensor.engMax, kGaugeStartDeg,
                                    kGaugeStartDeg + kGaugeSweepDeg);
  const float warnHighDeg = mapRange(sensor.warnHigh, sensor.engMin, sensor.engMax, kGaugeStartDeg,
                                     kGaugeStartDeg + kGaugeSweepDeg);
  drawGaugeArc(centerX, centerY, radius, 12, kGaugeStartDeg, warnLowDeg, TFT_RED);
  drawGaugeArc(centerX, centerY, radius, 12, warnLowDeg, warnHighDeg, TFT_DARKGREEN);
  drawGaugeArc(centerX, centerY, radius, 12, warnHighDeg, kGaugeStartDeg + kGaugeSweepDeg, TFT_ORANGE);

  for (uint8_t tick = 0; tick <= 10; ++tick) {
    const float angleDeg = kGaugeStartDeg + (kGaugeSweepDeg * tick / 10.0f);
    const float angleRad = angleDeg * DEG_TO_RAD;
    const int16_t x0 = centerX + (radius - 18) * cosf(angleRad);
    const int16_t y0 = centerY + (radius - 18) * sinf(angleRad);
    const int16_t x1 = centerX + (radius - 2) * cosf(angleRad);
    const int16_t y1 = centerY + (radius - 2) * sinf(angleRad);
    tft_.drawLine(x0, y0, x1, y1, TFT_LIGHTGREY);
  }

  const float clamped = constrain(sensor.filteredValue, sensor.engMin, sensor.engMax);
  const float angleDeg =
      mapRange(clamped, sensor.engMin, sensor.engMax, kGaugeStartDeg, kGaugeStartDeg + kGaugeSweepDeg);
  const float angleRad = angleDeg * DEG_TO_RAD;
  const int16_t needleX = centerX + (radius - 26) * cosf(angleRad);
  const int16_t needleY = centerY + (radius - 26) * sinf(angleRad);

  tft_.fillCircle(centerX, centerY, radius - 40, TFT_BLACK);
  tft_.drawCircle(centerX, centerY, radius - 40, TFT_DARKGREY);
  tft_.drawLine(centerX, centerY, needleX, needleY, valueColor(sensor));
  tft_.fillCircle(centerX, centerY, 6, TFT_WHITE);

  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.drawString(sensor.name, centerX, centerY - 30, 2);
  tft_.setTextColor(valueColor(sensor), TFT_BLACK);
  tft_.drawString(String(sensor.filteredValue, 1), centerX, centerY + 4, 7);
  tft_.setTextColor(TFT_WHITE, TFT_BLACK);
  tft_.drawString(sensor.units, centerX, centerY + 42, 2);
  tft_.drawString(String(sensor.minValue, 1) + " / " + String(sensor.maxValue, 1), centerX,
                  centerY + 66, 2);
}

void Dashboard::drawGaugeArc(const int16_t centerX,
                             const int16_t centerY,
                             const int16_t radius,
                             const int16_t thickness,
                             const float startDeg,
                             const float endDeg,
                             const uint16_t color) {
  for (int16_t offset = 0; offset < thickness; ++offset) {
    for (float angle = startDeg; angle <= endDeg; angle += 2.0f) {
      const float radians = angle * DEG_TO_RAD;
      const int16_t x = centerX + (radius - offset) * cosf(radians);
      const int16_t y = centerY + (radius - offset) * sinf(radians);
      tft_.drawPixel(x, y, color);
    }
  }
}

void Dashboard::drawStatusLine(const int16_t x,
                               const int16_t y,
                               const char *label,
                               const String &value,
                               const uint16_t color) {
  tft_.setTextDatum(TL_DATUM);
  tft_.setTextColor(TFT_LIGHTGREY, AppConfig::kDisplay.backgroundColor);
  tft_.drawString(String(label) + ":", x, y, 2);
  tft_.setTextColor(color, AppConfig::kDisplay.backgroundColor);
  tft_.drawString(value, x + 90, y, 2);
  tft_.setTextDatum(MC_DATUM);
}

uint16_t Dashboard::valueColor(const SensorSnapshot &sensor) const {
  if (sensor.activeFault != SensorFault::None) {
    return TFT_RED;
  }
  if (sensor.filteredValue < sensor.warnLow || sensor.filteredValue > sensor.warnHigh) {
    return TFT_ORANGE;
  }
  return TFT_GREENYELLOW;
}
