#pragma once

#include <Arduino.h>
#include <WebServer.h>
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

  WebServer server_{80};
  CsvLogger *logger_ = nullptr;
  AppState state_{};
  bool ready_ = false;
  String mode_;
  String ipAddress_;
};
