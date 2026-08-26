#pragma once

#include <Arduino.h>
#if defined(ESP8266)
#include <ESP8266WebServer.h>
using LoggerWebServer = ESP8266WebServer;
#else
#include <WebServer.h>
using LoggerWebServer = WebServer;
#endif
#include "AppConfig.h"
#include "CsvLogger.h"
#include "Types.h"

class WebUi {
 public:
  bool begin(const AppConfig::WifiConfig &config, CsvLogger &logger);
  void handleClient();
  void publishState(const AppState &state);
  bool isReady() const;
  String modeString() const;
  String ipAddress() const;

 private:
  void registerRoutes();
  void handleIndex();
  void handleLiveJson();
  void handleFilesJson();
  void handleDownload();
  String liveJson() const;
  String indexHtml() const;
  String sensorCardsHtml() const;

  LoggerWebServer server_{80};
  CsvLogger *logger_ = nullptr;
  AppState state_{};
  bool ready_ = false;
  String mode_;
  String ipAddress_;
};
