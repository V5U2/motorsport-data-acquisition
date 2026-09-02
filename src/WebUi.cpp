#include "WebUi.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

bool WebUi::begin(const AppConfig::WifiConfig &config,
                  CsvLogger &logger,
                  RuntimeSettings &settings) {
  logger_ = &logger;
  settings_ = &settings;

  if (config.mode == AppConfig::WifiMode::Station && strlen(config.stationSsid) > 0) {
    WiFi.persistent(false);
#if defined(ESP8266)
    WiFi.hostname("mda-logger");
#else
    WiFi.setHostname("mda-logger");
#endif
    WiFi.mode(WIFI_STA);
    Serial.print("STA joining ");
    Serial.println(config.stationSsid);
    WiFi.begin(config.stationSsid, config.stationPassword);
    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED &&
           (millis() - startMs) < (config.connectTimeoutSeconds * 1000UL)) {
      delay(100);
    }
    ready_ = WiFi.status() == WL_CONNECTED;

    if (ready_) {
      mode_ = "STA";
      ipAddress_ = WiFi.localIP().toString();
      Serial.print("STA connected ip=");
      Serial.println(ipAddress_);
    } else {
      Serial.print("STA failed status=");
      Serial.println(static_cast<int>(WiFi.status()));
    }
  }

  if (!ready_) {
    WiFi.persistent(false);
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    delay(100);
    WiFi.mode(WIFI_AP);
#if defined(ESP8266)
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#else
    WiFi.setSleep(false);
#endif

    const IPAddress apIp(config.apAddress[0], config.apAddress[1], config.apAddress[2],
                         config.apAddress[3]);
    WiFi.softAPConfig(apIp, apIp, IPAddress(255, 255, 255, 0));
    const uint8_t channel = config.apChannel == 0 ? 6 : config.apChannel;
    const char *passphrase = strlen(config.apPassword) >= 8 ? config.apPassword : nullptr;
    ready_ = WiFi.softAP(config.apSsid, passphrase, channel, 0, 4);
    delay(150);
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

void WebUi::handleClient() {
  server_.handleClient();
  if (restartPending_ && (millis() - restartRequestedMs_) >= 750) {
    ESP.restart();
  }
}

void WebUi::publishState(const AppState &state) { state_ = state; }

bool WebUi::isReady() const { return ready_; }

String WebUi::modeString() const { return mode_; }

String WebUi::ipAddress() const { return ipAddress_; }

void WebUi::setManagementPairingCode(const String &pairingCode,
                                     const uint32_t expiresInSeconds) {
  managementPairingCode_ = pairingCode;
  managementPairingExpiresInSeconds_ = expiresInSeconds;
}

void WebUi::registerRoutes() {
  server_.on("/", HTTP_GET, [this]() { handleIndex(); });
  server_.on("/api/live", HTTP_GET, [this]() { handleLiveJson(); });
  server_.on("/api/files", HTTP_GET, [this]() { handleFilesJson(); });
  server_.on("/settings", HTTP_GET, [this]() { handleSettings(); });
  server_.on("/settings", HTTP_POST, [this]() { handleSettingsSave(); });
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

void WebUi::handleSettings() {
  if (!settingsAuthorized()) {
    return;
  }

  const AppConfig::UploadConfig &upload = settings_->uploadConfig();
  String html = R"rawliteral(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Logger Settings</title><style>body{font-family:system-ui;max-width:38rem;margin:2rem auto;padding:0 1rem;background:#09131f;color:#ecf2f8}label{display:block;margin:1rem 0}.hint{color:#95a8ba}input{box-sizing:border-box;width:100%;padding:.7rem;margin-top:.3rem}input[type=checkbox]{width:auto}button{padding:.8rem 1.2rem}a{color:#6dd6ff}h2{margin-top:2rem}</style></head><body><h1>Device settings</h1><form method="post" action="/settings"><h2>Upstream server</h2><p class="hint">MQTT credentials remain in the local secrets header and are never returned by this page.</p><label><input type="checkbox" name="enabled" value="1")rawliteral";
  if (settings_->liveUploadEnabled()) {
    html += " checked";
  }
  html += R"rawliteral(> Enable live upload</label><label><input type="checkbox" name="remote_management" value="1")rawliteral";
  if (settings_->remoteManagementEnabled()) {
    html += " checked";
  }
  html += R"rawliteral(> Allow remote management</label><p class="hint">Optional. Pair and configure this logger from the ApexiLabs app. Live upload must remain enabled; disable this locally at any time to restore outbound-only operation.</p>)rawliteral";
  if (settings_->remoteManagementEnabled() && !managementPairingCode_.isEmpty()) {
    html += "<p>Temporary pairing code: <strong class=\"pairing-code\">" +
            htmlEscape(managementPairingCode_) + "</strong></p>";
    html += "<p class=\"hint\">Enter only this code in the ApexiLabs app. The app identifies this logger automatically. "
            "This code refreshes every 10 minutes; next refresh in <span id=\"pairingCountdown\" data-seconds=\"" +
            String(managementPairingExpiresInSeconds_) + "\">--:--</span>.</p>";
    html += R"rawliteral(<script>(()=>{const el=document.getElementById('pairingCountdown');let remaining=Number(el.dataset.seconds||0);const render=()=>{const minutes=Math.floor(remaining/60);const seconds=remaining%60;el.textContent=String(minutes).padStart(2,'0')+':'+String(seconds).padStart(2,'0');};render();setInterval(()=>{remaining=Math.max(0,remaining-1);render();if(remaining===0)window.location.reload();},1000);})();</script>)rawliteral";
  }
  html += R"rawliteral(<label>Server host<input name="host" required maxlength="63" value=")rawliteral";
  html += htmlEscape(upload.mqttHost);
  html += R"rawliteral("></label><label>MQTT port<input name="port" type="number" min="1" max="65535" required value=")rawliteral";
  html += String(upload.mqttPort);
  html += R"rawliteral("></label><h2>Network time</h2><label>Primary NTP server<input name="ntp_primary" required maxlength="63" value=")rawliteral";
  html += htmlEscape(settings_->ntpPrimary());
  html += R"rawliteral("></label><label>Secondary NTP server<input name="ntp_secondary" required maxlength="63" value=")rawliteral";
  html += htmlEscape(settings_->ntpSecondary());
  html += R"rawliteral("></label><label>Timezone rule<input name="tz_rule" required maxlength="63" value=")rawliteral";
  html += htmlEscape(settings_->timeZoneRule());
  html += R"rawliteral("></label><p class="hint">POSIX format examples: Perth <code>AWST-8</code>, UTC <code>UTC0</code>, Sydney <code>AEST-10AEDT,M10.1.0,M4.1.0/3</code>.</p><label>Timezone label<input name="tz_label" required maxlength="31" value=")rawliteral";
  html += htmlEscape(settings_->timeZoneLabel());
  html += R"rawliteral("></label><button type="submit">Save and restart</button></form><p><a href="/">Back to status</a></p></body></html>)rawliteral";
  server_.send(200, "text/html", html);
}

void WebUi::handleSettingsSave() {
  if (!settingsAuthorized()) {
    return;
  }

  const String host = server_.arg("host");
  const long portValue = server_.arg("port").toInt();
  const bool uploadEnabled = server_.hasArg("enabled");
  const bool remoteManagementEnabled = server_.hasArg("remote_management");
  if (remoteManagementEnabled && !uploadEnabled) {
    server_.send(400, "text/plain", "Remote management requires live upload");
    return;
  }
  if (portValue < 1 || portValue > 65535 ||
      !settings_->save(host,
                       static_cast<uint16_t>(portValue),
                       uploadEnabled,
                       server_.arg("ntp_primary"),
                       server_.arg("ntp_secondary"),
                       server_.arg("tz_rule"),
                       server_.arg("tz_label"),
                       remoteManagementEnabled,
                       settings_->appliedConfigVersion())) {
    server_.send(400, "text/plain", "Invalid settings");
    return;
  }

  server_.send(200, "text/html",
               "<!doctype html><meta name=viewport content='width=device-width'><p>Settings saved. The logger is restarting...</p>");
  restartPending_ = true;
  restartRequestedMs_ = millis();
}

bool WebUi::settingsAuthorized() {
  if (strlen(AppConfig::kOta.password) == 0) {
    server_.send(503, "text/plain", "Configure APEXI_OTA_PASSWORD before using settings");
    return false;
  }
  if (!server_.authenticate("admin", AppConfig::kOta.password)) {
    server_.requestAuthentication(DIGEST_AUTH, "MDA Logger");
    return false;
  }
  return true;
}

String WebUi::htmlEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    switch (value[index]) {
      case '&': escaped += "&amp;"; break;
      case '<': escaped += "&lt;"; break;
      case '>': escaped += "&gt;"; break;
      case '"': escaped += "&quot;"; break;
      case '\'': escaped += "&#39;"; break;
      default: escaped += value[index]; break;
    }
  }
  return escaped;
}

String WebUi::jsonEscape(const String &value) {
  String escaped;
  escaped.reserve(value.length() + 8);
  for (size_t index = 0; index < value.length(); ++index) {
    const char character = value[index];
    switch (character) {
      case '"': escaped += "\\\""; break;
      case '\\': escaped += "\\\\"; break;
      case '\b': escaped += "\\b"; break;
      case '\f': escaped += "\\f"; break;
      case '\n': escaped += "\\n"; break;
      case '\r': escaped += "\\r"; break;
      case '\t': escaped += "\\t"; break;
      default:
        if (static_cast<uint8_t>(character) >= 0x20) {
          escaped += character;
        }
        break;
    }
  }
  return escaped;
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
  json += "\"uptime\":\"" + state_.uptime + "\",";
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
  json += "\"display_enabled\":" + String(state_.system.displayEnabled ? "true" : "false") + ",";
  json += "\"rtc_enabled\":" + String(state_.system.rtcEnabled ? "true" : "false") + ",";
  json += "\"rtc_ready\":" + String(state_.system.rtcReady ? "true" : "false") + ",";
  json += "\"rtc_synced\":" + String(state_.system.rtcSynced ? "true" : "false") + ",";
  json += "\"rtc_error\":\"" + jsonEscape(state_.system.rtcError) + "\",";
  json += "\"rtc_last_sync\":\"" + jsonEscape(state_.system.rtcLastSync) + "\",";
  json += "\"time_zone\":\"" + jsonEscape(state_.system.timeZone) + "\",";
  json += "\"sd_enabled\":" + String(state_.system.sdEnabled ? "true" : "false") + ",";
  json += "\"sd_ready\":" + String(state_.system.sdReady ? "true" : "false") + ",";
  json += "\"wifi_ready\":" + String(state_.system.wifiReady ? "true" : "false") + ",";
  json += "\"upload_enabled\":" + String(state_.system.uploadEnabled ? "true" : "false") + ",";
  json += "\"upload_connected\":" + String(state_.system.uploadConnected ? "true" : "false") + ",";
  json += "\"ota_enabled\":" + String(state_.system.otaEnabled ? "true" : "false") + ",";
  json += "\"ota_ready\":" + String(state_.system.otaReady ? "true" : "false") + ",";
  json += "\"wifi_mode\":\"" + state_.system.wifiMode + "\",";
  json += "\"ip_address\":\"" + state_.system.ipAddress + "\",";
  json += "\"current_log_file\":\"" + state_.system.currentLogFile + "\",";
  json += "\"last_log_error\":\"" + jsonEscape(state_.system.lastLogError) + "\",";
  json += "\"upload_protocol\":\"" + state_.system.uploadProtocol + "\",";
  json += "\"upload_server\":\"" + jsonEscape(state_.system.uploadServer) + "\",";
  json += "\"upload_session_id\":\"" + state_.system.uploadSessionId + "\",";
  json += "\"upload_sequence\":" + String(state_.system.lastUploadSequence) + ",";
  json += "\"last_upload_error\":\"" + jsonEscape(state_.system.lastUploadError) + "\"}}";
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
    <h1>Motorsport Sensor Logger</h1>
    <div id="stamp">Waiting for data...</div>
  </header>
  <section class="grid">
)rawliteral";

  const String htmlEnd = R"rawliteral(
    <div class="card">
      <div class="label">System</div>
      <div class="status"><span>ADC</span><span id="adcStatus">--</span></div>
      <div class="status"><span>RTC</span><span id="rtcStatus">--</span></div>
      <div class="status"><span>Last time sync</span><span id="rtcLastSync">--</span></div>
      <div class="status"><span>SD</span><span id="sdStatus">--</span></div>
      <div class="status"><span>Upload</span><span id="uploadStatus">--</span></div>
      <div class="status"><span>Server</span><span id="uploadServer">--</span></div>
      <div class="status"><span>OTA</span><span id="otaStatus">--</span></div>
      <div class="status"><span>Wi-Fi</span><span id="wifiStatus">--</span></div>
      <div class="status"><span>Log file</span><span id="logFile">--</span></div>
      <div class="status"><span>Configuration</span><span><a href="/settings">Settings</a></span></div>
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
      document.getElementById('stamp').textContent = data.timestamp + ' ' + data.system.time_zone + ' | uptime ' + data.uptime;
      data.sensors.forEach((sensor) => {
        document.getElementById('sensor-value-' + sensor.id).textContent = sensor.value.toFixed(1) + ' ' + sensor.units;
        document.getElementById('sensor-loop-' + sensor.id).textContent = sensor.loop_mA.toFixed(2) + ' mA';
        document.getElementById('sensor-fault-' + sensor.id).textContent = sensor.fault;
      });
      document.getElementById('adcStatus').textContent = data.system.adc_ready ? 'OK' : 'FAULT';
      document.getElementById('rtcStatus').textContent = data.system.rtc_enabled ? (data.system.rtc_ready ? (data.system.rtc_synced ? 'NTP SYNCED' : 'HOLDOVER') : 'FAULT') : 'DISABLED';
      document.getElementById('rtcLastSync').textContent = data.system.rtc_last_sync || '--';
      document.getElementById('sdStatus').textContent = data.system.sd_enabled ? (data.system.sd_ready ? 'OK' : 'FAULT') : 'DISABLED';
      document.getElementById('uploadStatus').textContent = data.system.upload_enabled ? (data.system.upload_connected ? 'MQTT LIVE' : 'WAITING') : 'DISABLED';
      document.getElementById('uploadServer').textContent = data.system.upload_server || 'Not configured';
      document.getElementById('otaStatus').textContent = data.system.ota_enabled ? (data.system.ota_ready ? 'READY' : 'LOCKED') : 'DISABLED';
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
