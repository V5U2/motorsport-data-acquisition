#include <Adafruit_ADS1X15.h>
#include <ArduinoOTA.h>
#include <array>
#include <SPI.h>
#include <time.h>
#include <Wire.h>

#include "AppConfig.h"
#include "CsvLogger.h"
#include "Dashboard.h"
#include "LiveUpload.h"
#include "Logic.h"
#include "RuntimeSettings.h"
#include "SensorChannel.h"
#include "Timekeeper.h"
#include "WebUi.h"

namespace {

Adafruit_ADS1115 ads;
SPIClass &spiBus = SPI;
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
LiveUpload liveUpload;
RuntimeSettings runtimeSettings;

bool adcReady = false;
bool rtcReady = false;
bool rtcNetworkSynced = false;
bool wifiReady = false;
bool otaReady = false;
bool networkTimeConfigured = false;
String rtcLastSync;

uint32_t lastSampleMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastLogMs = 0;
uint32_t lastUploadPublishMs = 0;
uint32_t lastSerialMs = 0;

bool buttonLastLevel = HIGH;
uint32_t buttonPressedAtMs = 0;
bool longPressHandled = false;

constexpr uint32_t kValidNetworkEpoch = 1704067200UL;  // 2024-01-01 UTC
constexpr uint32_t kNtpSyncTimeoutMs = 10000;
constexpr uint32_t kRtcSyncRetryIntervalMs = 60000;
constexpr uint32_t kRtcResyncIntervalMs = 60UL * 60UL * 1000UL;
uint32_t lastRtcSyncAttemptMs = 0;
uint32_t lastRtcSuccessfulSyncMs = 0;

uint64_t extendedUptimeMs() {
  static uint32_t previousMs = 0;
  static uint64_t wrapBaseMs = 0;
  const uint32_t currentMs = millis();
  if (currentMs < previousMs) {
    wrapBaseMs += (1ULL << 32);
  }
  previousMs = currentMs;
  return wrapBaseMs + currentMs;
}

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
  const uint64_t uptimeMs = extendedUptimeMs();
  state.uptimeMs = static_cast<uint32_t>(uptimeMs);
  state.uptime = String(Logic::formatUptime(uptimeMs).c_str());
  state.timestamp = timekeeper.logTimestamp(state.uptimeMs);
  state.transportTimestamp = timekeeper.transportTimestamp(state.uptimeMs);
  state.system.adcReady = adcReady;
  state.system.displayEnabled = AppConfig::kFeatures.displayEnabled;
  state.system.rtcEnabled = AppConfig::kFeatures.rtcEnabled;
  state.system.rtcReady = rtcReady;
  state.system.rtcSynced = rtcNetworkSynced;
  state.system.rtcError = timekeeper.lastError();
  state.system.rtcLastSync = rtcLastSync;
  state.system.timeZone = runtimeSettings.timeZoneLabel();
  state.system.sdEnabled = AppConfig::kFeatures.sdLoggingEnabled;
  state.system.sdReady = csvLogger.isReady() && csvLogger.lastError().isEmpty();
  state.system.wifiReady = wifiReady;
  state.system.uploadEnabled = liveUpload.isEnabled();
  state.system.uploadConnected = liveUpload.isConnected();
  state.system.otaEnabled = AppConfig::kFeatures.otaUpdatesEnabled;
  state.system.otaReady = otaReady;
  state.system.wifiMode = webUi.modeString();
  state.system.ipAddress = webUi.ipAddress();
  state.system.currentLogFile = csvLogger.currentFileName();
  state.system.lastLogError = csvLogger.lastError();
  state.system.uploadProtocol = liveUpload.protocolName();
  state.system.uploadServer = liveUpload.serverName();
  state.system.uploadSessionId = liveUpload.sessionId();
  state.system.lastUploadError = liveUpload.lastError();
  state.system.lastUploadSequence = liveUpload.lastSequence();
  state.system.remoteManagementEnabled = runtimeSettings.remoteManagementEnabled();
  state.system.appliedConfigVersion = runtimeSettings.appliedConfigVersion();
  state.system.remoteManagementStatus = liveUpload.managementStatus();
  state.system.remoteManagementError = liveUpload.managementError();
  state.system.storeForwardEnabled = liveUpload.storeForwardEnabled();
  state.system.storeForwardReady = liveUpload.storeForwardReady();
  state.system.storeForwardPendingRecords = liveUpload.storeForwardPendingRecords();
  state.system.storeForwardPendingBytes = liveUpload.storeForwardPendingBytes();
  state.system.storeForwardCapacityBytes = liveUpload.storeForwardCapacityBytes();
  state.system.storeForwardDroppedRecords = liveUpload.storeForwardDroppedRecords();
  state.system.storeForwardCorruptionEvents = liveUpload.storeForwardCorruptionEvents();
  state.system.storeForwardQuarantinedBytes = liveUpload.storeForwardQuarantinedBytes();
  state.system.storeForwardError = liveUpload.storeForwardError();
  return state;
}

void sampleSensors() {
  for (size_t index = 0; index < sensorChannels.size(); ++index) {
    const AppConfig::SensorConfig &config = AppConfig::kSensorConfigs[index];
    const float voltage = adcReady ? readVoltage(config.adsChannel) : 0.0f;
    sensorChannels[index].update(voltage, adcReady);
  }
}

void beginOta() {
  if (!AppConfig::kFeatures.otaUpdatesEnabled || !wifiReady ||
      webUi.modeString() != "STA" || strlen(AppConfig::kOta.password) == 0) {
    otaReady = false;
    return;
  }

  ArduinoOTA.setHostname(AppConfig::kOta.hostname);
  ArduinoOTA.setPassword(AppConfig::kOta.password);
  ArduinoOTA.onStart([]() { Serial.println("OTA update started"); });
  ArduinoOTA.onEnd([]() { Serial.println("OTA update complete"); });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.print("OTA error=");
    Serial.println(static_cast<unsigned int>(error));
  });
  ArduinoOTA.begin();
  otaReady = true;
}

bool syncRtcFromNetwork(const bool waitForInitialSync) {
  if (!AppConfig::kFeatures.rtcEnabled || !wifiReady ||
      webUi.modeString() != "STA") {
    return false;
  }

  if (!networkTimeConfigured) {
#if defined(ESP8266)
    configTime(runtimeSettings.timeZoneRule(),
               runtimeSettings.ntpPrimary(),
               runtimeSettings.ntpSecondary());
#else
    configTzTime(runtimeSettings.timeZoneRule(),
                 runtimeSettings.ntpPrimary(),
                 runtimeSettings.ntpSecondary());
#endif
    networkTimeConfigured = true;
  }

  const uint32_t startedMs = millis();
  lastRtcSyncAttemptMs = startedMs;
  time_t now = time(nullptr);
  while (waitForInitialSync &&
         now < static_cast<time_t>(kValidNetworkEpoch) &&
         (millis() - startedMs) < kNtpSyncTimeoutMs) {
    ArduinoOTA.handle();
    webUi.handleClient();
    delay(100);
    now = time(nullptr);
  }

  if (now >= static_cast<time_t>(kValidNetworkEpoch)) {
    rtcReady = timekeeper.setFromUnixTime(static_cast<uint32_t>(now));
    if (rtcReady) {
      rtcNetworkSynced = true;
      lastRtcSuccessfulSyncMs = millis();
      rtcLastSync = timekeeper.logTimestamp(lastRtcSuccessfulSyncMs) + " " +
                    runtimeSettings.timeZoneLabel();
      return true;
    }
  }
  return false;
}

void maintainRtcSync(const uint32_t nowMs) {
  if (!AppConfig::kFeatures.rtcEnabled || !wifiReady ||
      webUi.modeString() != "STA") {
    return;
  }
  const uint32_t previousMs =
      rtcNetworkSynced ? lastRtcSuccessfulSyncMs : lastRtcSyncAttemptMs;
  const uint32_t intervalMs =
      rtcNetworkSynced ? kRtcResyncIntervalMs : kRtcSyncRetryIntervalMs;
  if (Logic::intervalElapsed(nowMs, previousMs, intervalMs)) {
    syncRtcFromNetwork(false);
  }
}

}  // namespace

void setup() {
  Serial.begin(115200);
  const uint32_t serialWaitStartMs = millis();
  while (!Serial && (millis() - serialWaitStartMs) < 5000) {
    delay(10);
  }
  Serial.println("MDA logger boot");
  Serial.flush();

  // A steady light confirms that the MCU has reached firmware setup.
  pinMode(AppConfig::kPins.statusLed, OUTPUT);
#if defined(ESP8266)
  digitalWrite(AppConfig::kPins.statusLed, LOW);
#else
  digitalWrite(AppConfig::kPins.statusLed, HIGH);
#endif

  pinMode(AppConfig::kPins.buttonPin, INPUT_PULLUP);
  if (AppConfig::kFeatures.displayEnabled) {
    pinMode(AppConfig::kPins.tftCs, OUTPUT);
    digitalWrite(AppConfig::kPins.tftCs, HIGH);
  }
  if (AppConfig::kFeatures.sdLoggingEnabled) {
    pinMode(AppConfig::kPins.sdCs, OUTPUT);
    digitalWrite(AppConfig::kPins.sdCs, HIGH);
  }

  if (!AppConfig::kFeatures.rtcEnabled) {
    timekeeper.disable();
    rtcReady = false;
  }
  if (!AppConfig::kFeatures.sdLoggingEnabled) {
    csvLogger.disable();
  }

  runtimeSettings.begin(AppConfig::kLiveUpload, AppConfig::kFeatures.liveUploadEnabled);
  wifiReady = webUi.begin(AppConfig::kWifi, csvLogger, runtimeSettings);
  liveUpload.begin(runtimeSettings.uploadConfig(),
                   runtimeSettings.liveUploadEnabled(),
                   runtimeSettings.remoteManagementEnabled(),
                   runtimeSettings.appliedConfigVersion());
  liveUpload.setReportedConfig(runtimeSettings.liveUploadEnabled(),
                               runtimeSettings.ntpPrimary(),
                               runtimeSettings.ntpSecondary(),
                               runtimeSettings.timeZoneRule(),
                               runtimeSettings.timeZoneLabel());
  Serial.print("storeForwardReady=");
  Serial.print(liveUpload.storeForwardReady() ? "1" : "0");
  Serial.print(" pending=");
  Serial.print(liveUpload.storeForwardPendingRecords());
  Serial.print(" capacityBytes=");
  Serial.println(liveUpload.storeForwardCapacityBytes());
  webUi.setManagementPairingCode(liveUpload.pairingCode(),
                                 liveUpload.pairingCodeExpiresInSeconds());
  Serial.print("wifiReady=");
  Serial.println(wifiReady ? "1" : "0");
  Serial.print("wifiMode=");
  Serial.println(webUi.modeString());
  Serial.print("ip=");
  Serial.println(webUi.ipAddress());
  beginOta();
  Serial.print("otaReady=");
  Serial.println(otaReady ? "1" : "0");

  Wire.begin(AppConfig::kPins.i2cSda, AppConfig::kPins.i2cScl);
  spiBus.begin();

  dashboard.begin();

  adcReady = ads.begin(AppConfig::kAds1115Address, &Wire);
  if (adcReady) {
    ads.setGain(GAIN_ONE);
    ads.setDataRate(RATE_ADS1115_860SPS);
  }

  if (AppConfig::kFeatures.rtcEnabled) {
    rtcReady = timekeeper.begin(Wire, AppConfig::kRtc);
    syncRtcFromNetwork(true);
  }

  if (AppConfig::kFeatures.sdLoggingEnabled) {
    csvLogger.begin(AppConfig::kPins.sdCs, spiBus);
  }

  Serial.print("adcReady=");
  Serial.println(adcReady ? "1" : "0");

  sampleSensors();
  const AppState initialState = buildState();
  dashboard.render(initialState);
  webUi.publishState(initialState);
}

void loop() {
  handleButton();
  webUi.handleClient();
  liveUpload.loop();
  webUi.setManagementPairingCode(liveUpload.pairingCode(),
                                 liveUpload.pairingCodeExpiresInSeconds());
  RemoteConfig remoteConfig{};
  if (liveUpload.consumeRemoteConfig(remoteConfig)) {
    if (runtimeSettings.applyRemoteConfig(remoteConfig)) {
      liveUpload.setReportedConfig(runtimeSettings.liveUploadEnabled(),
                                   runtimeSettings.ntpPrimary(),
                                   runtimeSettings.ntpSecondary(),
                                   runtimeSettings.timeZoneRule(),
                                   runtimeSettings.timeZoneLabel());
      liveUpload.acknowledgeRemoteConfig(remoteConfig.version);
      Serial.print("remoteConfigApplied=");
      Serial.println(remoteConfig.version);
      Serial.flush();
      delay(250);
      ESP.restart();
    }
  }
  if (otaReady) {
    ArduinoOTA.handle();
  }

  const uint32_t nowMs = millis();
  maintainRtcSync(nowMs);

  if ((nowMs - lastSampleMs) >= AppConfig::kTiming.sampleIntervalMs) {
    sampleSensors();
    lastSampleMs = nowMs;
  }

  if ((nowMs - lastLogMs) >= AppConfig::kTiming.loggingIntervalMs) {
    AppState state = buildState();
    if (AppConfig::kFeatures.sdLoggingEnabled && csvLogger.isReady()) {
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

  if ((nowMs - lastUploadPublishMs) >= AppConfig::kLiveUpload.publishIntervalMs) {
    const AppState state = buildState();
    liveUpload.publishIfDue(state);
    webUi.publishState(buildState());
    lastUploadPublishMs = nowMs;
  }

  if ((nowMs - lastSerialMs) >= 1000) {
    const AppState state = buildState();
    Serial.print("IP_ADDRESS=");
    Serial.print(state.system.ipAddress);
    Serial.print(" MODE=");
    Serial.print(state.system.wifiMode);
    Serial.print(" adc=");
    Serial.print(state.system.adcReady ? 1 : 0);
    Serial.print(" wifi=");
    Serial.print(state.system.wifiReady ? 1 : 0);
    Serial.print(" ip=");
    Serial.print(state.system.ipAddress);
    for (const SensorSnapshot &sensor : state.sensors) {
      Serial.print(" | ");
      Serial.print(sensor.id);
      Serial.print(" V=");
      Serial.print(sensor.rawVoltage, 3);
      Serial.print(" mA=");
      Serial.print(sensor.loopCurrentmA, 2);
      Serial.print(" val=");
      Serial.print(sensor.filteredValue, 2);
      Serial.print(" fault=");
      Serial.print(sensorFaultToString(sensor.activeFault));
    }
    Serial.println();
    Serial.flush();
    lastSerialMs = nowMs;
  }
}
