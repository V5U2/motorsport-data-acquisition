#pragma once

#include <array>
#include <Arduino.h>
#include "PinDefinitions.h"

namespace AppConfig {

constexpr float kShuntResistanceOhms = 165.0f;
constexpr uint8_t kAds1115Address = 0x48;
constexpr uint8_t kDs3231Address = 0x68;

struct SensorConfig {
  const char *id;
  const char *name;
  uint8_t adsChannel;
  float currentMinmA;
  float currentMaxmA;
  float engMin;
  float engMax;
  const char *units;
  float warnLow;
  float warnHigh;
  float filterAlpha;
};

enum class WifiMode : uint8_t {
  SoftAp,
  Station
};

struct TimingConfig {
  uint32_t sampleIntervalMs;
  uint32_t displayIntervalMs;
  uint32_t loggingIntervalMs;
  uint32_t loggerFlushIntervalMs;
  uint16_t loggerFlushRows;
};

struct WifiConfig {
  WifiMode mode;
  const char *apSsid;
  const char *apPassword;
  const char *stationSsid;
  const char *stationPassword;
  uint8_t connectTimeoutSeconds;
};

struct FeatureConfig {
  bool displayEnabled;
  bool rtcEnabled;
  bool sdLoggingEnabled;
};

struct PinConfig {
  uint8_t i2cSda;
  uint8_t i2cScl;
  uint8_t spiMosi;
  uint8_t spiMiso;
  uint8_t spiSclk;
  uint8_t tftCs;
  uint8_t tftDc;
  uint8_t tftRst;
  int8_t tftBacklight;
  uint8_t sdCs;
  uint8_t buttonPin;
};

struct DisplayConfig {
  uint8_t rotation;
  uint16_t backgroundColor;
  uint16_t foregroundColor;
};

inline constexpr PinConfig kPins{
    PIN_I2C_SDA,
    PIN_I2C_SCL,
    PIN_SPI_MOSI,
    PIN_SPI_MISO,
    PIN_SPI_SCLK,
    PIN_TFT_CS,
    PIN_TFT_DC,
    PIN_TFT_RST,
    PIN_TFT_BL,
    PIN_SD_CS,
    PIN_UI_BUTTON,
};

inline constexpr std::array<SensorConfig, 2> kSensorConfigs{{
    {
        "oil_pressure",
        "Oil Pressure",
        0,
        4.0f,
        20.0f,
        0.0f,
        10.0f,
        "bar",
        1.5f,
        8.5f,
        0.18f,
    },
    {
        "oil_temperature",
        "Oil Temp",
        1,
        4.0f,
        20.0f,
        0.0f,
        150.0f,
        "C",
        70.0f,
        125.0f,
        0.18f,
    },
}};

inline constexpr size_t kSensorCount = kSensorConfigs.size();

inline constexpr TimingConfig kTiming{
    10,
    100,
    50,
    1000,
    20,
};

inline constexpr WifiConfig kWifi{
    WifiMode::SoftAp,
    "MDA-LOGGER",
    "changeme1",
    "",
    "",
    10,
};

inline constexpr FeatureConfig kFeatures{
    true,
    true,
    true,
};

inline constexpr DisplayConfig kDisplay{
    1,
    0x1082,
    0xFFFF,
};

}  // namespace AppConfig
