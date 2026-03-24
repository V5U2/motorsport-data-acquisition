#include "WebUi.h"

#include <WiFi.h>

bool WebUi::begin(const AppConfig::WifiConfig &config, CsvLogger &logger) {
  logger_ = &logger;

  if (config.mode == AppConfig::WifiMode::Station && strlen(config.stationSsid) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.stationSsid, config.stationPassword);

    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - startMs) < (config.connectTimeoutSeconds * 1000UL)) {
      delay(100);
    }

    if (WiFi.status() == WL_CONNECTED) {
      ready_ = true;
      mode_ = "STA";
      ipAddress_ = WiFi.localIP().toString();
    }
  }

  if (!ready_) {
    WiFi.mode(WIFI_AP);
    ready_ = WiFi.softAP(config.apSsid, config.apPassword);
    mode_ = "AP";
    ipAddress_ = WiFi.softAPIP().toString();
  }

  if (!ready_) {
    mode_ = "OFF";
    ipAddress_ = "0.0.0.0";
    return false;
  }

  registerRoutes();
  server_.begin();
  return true;
}

void WebUi::handleClient() { server_.handleClient(); }

void WebUi::publishState(const AppState &state) { state_ = state; }

bool WebUi::isReady() const { return ready_; }

String WebUi::modeString() const { return mode_; }

String WebUi::ipAddress() const { return ipAddress_; }

void WebUi::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleIndex(); });
  server_.on("/api/live", HTTP_GET, [this]() { handleLiveJson(); });
  server_.on("/api/files", HTTP_GET, [this]() { handleFilesJson(); });
  server_.onNotFound([this]() { handleDownload(); });
}

void WebUi::handleIndex() { server_.send(200, "text/html", indexHtml()); }

void WebUi::handleLiveJson() { server_.send(200, "application/json", liveJson()); }

void WebUi::handleFilesJson() {
  if (logger_ == nullptr) {
    server_.send(503, "application/json", "[]");
    return;
  }
  server_.send(200, "application/json", logger_->listFilesJson());
}

void WebUi::handleDownload() {
  if (logger_ == nullptr || !server_.uri().startsWith("/download/")) {
    server_.send(404, "text/plain", "Not found");
    return;
  }

  const String fileName = server_.uri().substring(strlen("/download/"));
  File file = logger_->openReadOnly(fileName);
  if (!file) {
    server_.send(404, "text/plain", "File not found");
    return;
  }

  server_.streamFile(file, "text/csv");
  file.close();
}

String WebUi::liveJson() const {
  String json = "{";
  json += "\"timestamp\":\"" + state_.timestamp + "\",";
  json += "\"uptime_ms\":" + String(state_.uptimeMs) + ",";
  json += "\"sensors\":[";
  for (size_t index = 0; index < state_.sensors.size(); ++index) {
    const SensorSnapshot &sensor = state_.sensors[index];
    if (index > 0) {
      json += ",";
    }
    json += "{";
    json += "\"id\":\"" + String(sensor.id) + "\",";
    json += "\"name\":\"" + String(sensor.name) + "\",";
    json += "\"value\":" + String(sensor.filteredValue, 3) + ",";
    json += "\"units\":\"" + String(sensor.units) + "\",";
    json += "\"loop_mA\":" + String(sensor.loopCurrentmA, 3) + ",";
    json += "\"fault\":\"" + String(sensorFaultToString(sensor.activeFault)) + "\"}";
  }
  json += "],";
  json += "\"system\":{";
  json += "\"adc_ready\":" + String(state_.system.adcReady ? "true" : "false") + ",";
  json += "\"rtc_ready\":" + String(state_.system.rtcReady ? "true" : "false") + ",";
  json += "\"sd_ready\":" + String(state_.system.sdReady ? "true" : "false") + ",";
  json += "\"wifi_ready\":" + String(state_.system.wifiReady ? "true" : "false") + ",";
  json += "\"wifi_mode\":\"" + state_.system.wifiMode + "\",";
  json += "\"ip_address\":\"" + state_.system.ipAddress + "\",";
  json += "\"current_log_file\":\"" + state_.system.currentLogFile + "\"}}";
  return json;
}

String WebUi::sensorCardsHtml() const {
  String html;
  for (size_t index = 0; index < AppConfig::kSensorCount; ++index) {
    const AppConfig::SensorConfig &sensor = AppConfig::kSensorConfigs[index];
    html += "<div class=\"card sensor-card\" data-sensor-id=\"" + String(sensor.id) + "\">";
    html += "<div class=\"label\">" + String(sensor.name) + "</div>";
    html += "<div class=\"value\" id=\"sensor-value-" + String(sensor.id) + "\">--</div>";
    html += "<div class=\"status\"><span id=\"sensor-loop-" + String(sensor.id) +
            "\">--</span><span id=\"sensor-fault-" + String(sensor.id) + "\">--</span></div>";
    html += "</div>";
  }
  return html;
}

String WebUi::indexHtml() const {
  const String htmlStart = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Motorsport Logger</title>
  <style>
    body { margin: 0; font-family: "Segoe UI", sans-serif; background: #09131f; color: #ecf2f8; }
    header { padding: 18px 20px; background: linear-gradient(120deg, #10263a, #173b2d); }
    h1 { margin: 0; font-size: 1.4rem; }
    .grid { display: grid; gap: 16px; padding: 16px; grid-template-columns: repeat(auto-fit, minmax(240px, 1fr)); }
    .card { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.08); border-radius: 16px; padding: 16px; }
    .label { color: #95a8ba; font-size: 0.9rem; }
    .value { font-size: 2.2rem; margin-top: 6px; }
    .status { display: flex; justify-content: space-between; margin-top: 8px; font-size: 0.95rem; }
    ul { padding-left: 18px; }
    a { color: #6dd6ff; }
  </style>
</head>
<body>
  <header>
    <h1>ESP32 Sensor Logger</h1>
    <div id="stamp">Waiting for data...</div>
  </header>
  <section class="grid">
)rawliteral";

  const String htmlEnd = R"rawliteral(
    <div class="card">
      <div class="label">System</div>
      <div class="status"><span>ADC</span><span id="adcStatus">--</span></div>
      <div class="status"><span>RTC</span><span id="rtcStatus">--</span></div>
      <div class="status"><span>SD</span><span id="sdStatus">--</span></div>
      <div class="status"><span>Wi-Fi</span><span id="wifiStatus">--</span></div>
      <div class="status"><span>Log file</span><span id="logFile">--</span></div>
    </div>
    <div class="card">
      <div class="label">CSV Files</div>
      <ul id="fileList"></ul>
    </div>
  </section>
  <script>
    async function refreshLive() {
      const response = await fetch('/api/live');
      const data = await response.json();
      document.getElementById('stamp').textContent = data.timestamp + ' | uptime ' + data.uptime_ms + ' ms';
      data.sensors.forEach((sensor) => {
        document.getElementById('sensor-value-' + sensor.id).textContent = sensor.value.toFixed(1) + ' ' + sensor.units;
        document.getElementById('sensor-loop-' + sensor.id).textContent = sensor.loop_mA.toFixed(2) + ' mA';
        document.getElementById('sensor-fault-' + sensor.id).textContent = sensor.fault;
      });
      document.getElementById('adcStatus').textContent = data.system.adc_ready ? 'OK' : 'FAULT';
      document.getElementById('rtcStatus').textContent = data.system.rtc_ready ? 'OK' : 'FAULT';
      document.getElementById('sdStatus').textContent = data.system.sd_ready ? 'OK' : 'FAULT';
      document.getElementById('wifiStatus').textContent = data.system.wifi_mode + ' ' + data.system.ip_address;
      document.getElementById('logFile').textContent = data.system.current_log_file || '--';
    }

    async function refreshFiles() {
      const response = await fetch('/api/files');
      const files = await response.json();
      const target = document.getElementById('fileList');
      target.innerHTML = '';
      files.forEach((file) => {
        const li = document.createElement('li');
        const a = document.createElement('a');
        a.href = '/download/' + file.name.replace(/^\//, '');
        a.textContent = file.name + ' (' + file.size + ' B)';
        li.appendChild(a);
        target.appendChild(li);
      });
    }

    async function refreshAll() {
      try {
        await refreshLive();
        await refreshFiles();
      } catch (error) {
        document.getElementById('stamp').textContent = 'Web UI refresh failed';
      }
    }

    refreshAll();
    setInterval(refreshAll, 1000);
  </script>
</body>
</html>
)rawliteral";

  return htmlStart + sensorCardsHtml() + htmlEnd;
}
