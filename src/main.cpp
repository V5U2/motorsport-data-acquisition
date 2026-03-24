#include <Adafruit_ADS1X15.h>
#include <array>
#include <SPI.h>
#include <Wire.h>

#include "AppConfig.h"
#include "CsvLogger.h"
#include "Dashboard.h"
#include "SensorChannel.h"
#include "Timekeeper.h"
#include "WebUi.h"

namespace {

Adafruit_ADS1115 ads;
SPIClass spiBus(FSPI);
std::array<SensorChannel, AppConfig::kSensorCount> sensorChannels = [] {
  std::array<SensorChannel, AppConfig::kSensorCount> channels{};
  for (size_t index = 0; index < AppConfig::kSensorCount; ++index) {
    channels[index].configure(AppConfig::kSensorConfigs[index]);
  }
  return channels;
}();
Timekeeper timekeeper;
CsvLogger csvLogger;
Dashboard dashboard;
WebUi webUi;

bool adcReady = false;
bool rtcReady = false;
bool wifiReady = false;

uint32_t lastSampleMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastLogMs = 0;

bool buttonLastLevel = HIGH;
uint32_t buttonPressedAtMs = 0;
bool longPressHandled = false;

float readVoltage(const uint8_t channel) {
  const int16_t raw = ads.readADC_SingleEnded(channel);
  return ads.computeVolts(raw);
}

void clearFaults() {
  for (SensorChannel &sensor : sensorChannels) {
    sensor.clearLatchedFault();
  }
}

void handleButton() {
  const bool level = digitalRead(AppConfig::kPins.buttonPin);
  const uint32_t nowMs = millis();

  if (buttonLastLevel == HIGH && level == LOW) {
    buttonPressedAtMs = nowMs;
    longPressHandled = false;
  } else if (buttonLastLevel == LOW && level == LOW) {
    if (!longPressHandled && (nowMs - buttonPressedAtMs) >= 1200) {
      clearFaults();
      longPressHandled = true;
    }
  } else if (buttonLastLevel == LOW && level == HIGH) {
    if (!longPressHandled && (nowMs - buttonPressedAtMs) >= 30) {
      dashboard.nextScreen();
    }
  }

  buttonLastLevel = level;
}

AppState buildState() {
  AppState state{};
  for (size_t index = 0; index < sensorChannels.size(); ++index) {
    state.sensors[index] = sensorChannels[index].snapshot();
  }
  state.uptimeMs = millis();
  state.timestamp = timekeeper.logTimestamp(state.uptimeMs);
  state.system.adcReady = adcReady;
  state.system.rtcReady = rtcReady;
  state.system.sdReady = csvLogger.isReady() && csvLogger.lastError().isEmpty();
  state.system.wifiReady = wifiReady;
  state.system.wifiMode = webUi.modeString();
  state.system.ipAddress = webUi.ipAddress();
  state.system.currentLogFile = csvLogger.currentFileName();
  state.system.lastLogError = csvLogger.lastError();
  return state;
}

void sampleSensors() {
  for (size_t index = 0; index < sensorChannels.size(); ++index) {
    const AppConfig::SensorConfig &config = AppConfig::kSensorConfigs[index];
    const float voltage = adcReady ? readVoltage(config.adsChannel) : 0.0f;
    sensorChannels[index].update(voltage, adcReady);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);

  pinMode(AppConfig::kPins.buttonPin, INPUT_PULLUP);
  pinMode(AppConfig::kPins.tftCs, OUTPUT);
  pinMode(AppConfig::kPins.sdCs, OUTPUT);
  digitalWrite(AppConfig::kPins.tftCs, HIGH);
  digitalWrite(AppConfig::kPins.sdCs, HIGH);

  Wire.begin(AppConfig::kPins.i2cSda, AppConfig::kPins.i2cScl);
  spiBus.begin(AppConfig::kPins.spiSclk, AppConfig::kPins.spiMiso, AppConfig::kPins.spiMosi);

  dashboard.begin();

  adcReady = ads.begin(AppConfig::kAds1115Address, &Wire);
  if (adcReady) {
    ads.setGain(GAIN_ONE);
    ads.setDataRate(RATE_ADS1115_860SPS);
  }

  rtcReady = timekeeper.begin(Wire);
  csvLogger.begin(AppConfig::kPins.sdCs, spiBus);
  wifiReady = webUi.begin(AppConfig::kWifi, csvLogger);

  sampleSensors();
  const AppState initialState = buildState();
  dashboard.render(initialState);
  webUi.publishState(initialState);
}

void loop() {
  handleButton();
  webUi.handleClient();

  const uint32_t nowMs = millis();

  if ((nowMs - lastSampleMs) >= AppConfig::kTiming.sampleIntervalMs) {
    sampleSensors();
    lastSampleMs = nowMs;
  }

  if ((nowMs - lastLogMs) >= AppConfig::kTiming.loggingIntervalMs) {
    AppState state = buildState();
    if (csvLogger.isReady()) {
      if (csvLogger.logRow(timekeeper, state.uptimeMs, state.sensors)) {
        csvLogger.flushIfNeeded(state.uptimeMs);
      }
    }
    state.system.sdReady = csvLogger.isReady() && csvLogger.lastError().isEmpty();
    state.system.currentLogFile = csvLogger.currentFileName();
    state.system.lastLogError = csvLogger.lastError();
    webUi.publishState(state);
    lastLogMs = nowMs;
  }

  if ((nowMs - lastDisplayMs) >= AppConfig::kTiming.displayIntervalMs) {
    const AppState state = buildState();
    dashboard.render(state);
    webUi.publishState(state);
    lastDisplayMs = nowMs;
  }
}
