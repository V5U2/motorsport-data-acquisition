#pragma once

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "AppConfig.h"
#include "Types.h"

class Dashboard {
 public:
  enum class Screen : uint8_t {
    Main,
    Diagnostics,
  };

  bool begin();
  void render(const AppState &state);
  void nextScreen();
  Screen currentScreen() const;

 private:
  void drawMainScreen(const AppState &state);
  void drawDiagnosticsScreen(const AppState &state);
  void drawGauge(int16_t centerX,
                 int16_t centerY,
                 int16_t radius,
                 const SensorSnapshot &sensor);
  void drawGaugeArc(int16_t centerX,
                    int16_t centerY,
                    int16_t radius,
                    int16_t thickness,
                    float startDeg,
                    float endDeg,
                    uint16_t color);
  void drawStatusLine(int16_t x,
                      int16_t y,
                      const char *label,
                      const String &value,
                      uint16_t color);
  uint16_t valueColor(const SensorSnapshot &sensor) const;

  TFT_eSPI tft_;
  Screen screen_ = Screen::Main;
  bool ready_ = false;
};
