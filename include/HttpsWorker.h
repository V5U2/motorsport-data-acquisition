#pragma once

#if defined(ESP32)
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "HttpsExchange.h"

class HttpsWorker {
 public:
  bool begin(const char *rootCertificate);
  bool submit(const char *url, const char *payload, const char *bearer,
              const char *accessId, const char *accessSecret);
  const HttpsExchange::Result *result() const { return exchange_.result(); }
  void release() { exchange_.release(); }
  bool idle() const { return exchange_.idle(); }
 private:
  static void run(void *context);
  HttpsExchange exchange_;
  TaskHandle_t task_ = nullptr;
  const char *rootCertificate_ = nullptr;
};
#endif
