#pragma once

#include <array>
#include <Arduino.h>
#include "PinDefinitions.h"

#if __has_include("AppSecrets.h")
#include "AppSecrets.h"
#endif

#ifndef APEXI_WIFI_STATION_SSID
#define APEXI_WIFI_STATION_SSID ""
#endif
#ifndef APEXI_WIFI_STATION_PASSWORD
#define APEXI_WIFI_STATION_PASSWORD ""
#endif
#ifndef APEXI_MQTT_HOST
#define APEXI_MQTT_HOST ""
#endif
#ifndef APEXI_MQTT_USERNAME
#define APEXI_MQTT_USERNAME ""
#endif
#ifndef APEXI_MQTT_PASSWORD
#define APEXI_MQTT_PASSWORD ""
#endif

namespace AppConfig {

// DFRobot SEN0262 presents a 120 ohm sense path (0-25 mA -> 0-3 V).
constexpr float kShuntResistanceOhms = 120.0f;
constexpr uint8_t kAds1115Address = 0x48;
constexpr uint8_t kDs3231Address = 0x68;
constexpr uint8_t kRv3028Address = 0x52;

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

enum class RtcKind : uint8_t {
  Ds3231,
  Rv3028,
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
  uint8_t apAddress[4];
  uint8_t apChannel;
  const char *stationSsid;
  const char *stationPassword;
  uint8_t connectTimeoutSeconds;
};

struct FeatureConfig {
  bool displayEnabled;
  bool rtcEnabled;
  bool sdLoggingEnabled;
  bool liveUploadEnabled;
};

struct UploadConfig {
  const char *deviceId;
  const char *mqttHost;
  uint16_t mqttPort;
  const char *mqttUsername;
  const char *mqttPassword;
  const char *topicPrefix;
  uint32_t publishIntervalMs;
  uint16_t reconnectIntervalMs;
};

struct RtcConfig {
  RtcKind kind;
  uint8_t address;
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
    MDA_PIN_SPI_MOSI,
    MDA_PIN_SPI_MISO,
    MDA_PIN_SPI_SCLK,
    PIN_TFT_CS,
    PIN_TFT_DC,
    PIN_TFT_RST,
    PIN_TFT_BL,
    PIN_SD_CS,
    PIN_UI_BUTTON,
};

inline constexpr RtcConfig kRtc{
    RtcKind::Rv3028,
    kRv3028Address,
};

// Current bench configuration: pressure transmitter on ADS1115 A0 only.
inline constexpr std::array<SensorConfig, 1> kSensorConfigs{{
    {
        "oil_pressure",
        "Oil Pressure",
        0,
        4.0f,
        20.0f,
        0.0f,
        8.0f,
        "bar",
        1.5f,
        7.5f,
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
    WifiMode::Station,
    "MDA-LOGGER",
    "",
    {192, 168, 44, 1},
    6,
    APEXI_WIFI_STATION_SSID,
    APEXI_WIFI_STATION_PASSWORD,
    30,
};

inline constexpr FeatureConfig kFeatures{
    false,
    false,
    false,
    false,
};

inline constexpr UploadConfig kLiveUpload{
    "mda-logger",
    APEXI_MQTT_HOST,
    1883,
    APEXI_MQTT_USERNAME,
    APEXI_MQTT_PASSWORD,
    "motorsport/logger",
    250,
    5000,
};

inline constexpr DisplayConfig kDisplay{
    1,
    0x1082,
    0xFFFF,
};

}  // namespace AppConfig
