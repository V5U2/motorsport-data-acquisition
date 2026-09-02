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
  const bool httpsUpload = upload.protocol == AppConfig::UploadConfig::Protocol::Https;
  String html = R"rawliteral(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Logger Settings</title><style>body{font-family:system-ui;max-width:38rem;margin:2rem auto;padding:0 1rem;background:#09131f;color:#ecf2f8}label{display:block;margin:1rem 0}.hint{color:#95a8ba}input{box-sizing:border-box;width:100%;padding:.7rem;margin-top:.3rem}input[type=checkbox]{width:auto}button{padding:.8rem 1.2rem}a{color:#6dd6ff}h2{margin-top:2rem}</style></head><body><h1>Device settings</h1><form method="post" action="/settings"><h2>Upstream server</h2>)rawliteral";
  html += httpsUpload
              ? R"rawliteral(<p class="hint">HTTPS through Cloudflare Access. Credentials may be compiled into the firmware or replaced below. Stored values are never returned by this page.</p>)rawliteral"
              : R"rawliteral(<p class="hint">MQTT credentials remain in the local secrets header and are never returned by this page.</p>)rawliteral";
  html += "<p>Protocol: <strong>" + String(httpsUpload ? "HTTPS" : "MQTT") + "</strong></p>";
  html += R"rawliteral(<label><input type="checkbox" name="enabled" value="1")rawliteral";
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
  html += R"rawliteral("></label><label>)rawliteral";
  html += httpsUpload ? "HTTPS port" : "MQTT port";
  html += R"rawliteral(<input name="port" type="number" min="1" max="65535" required value=")rawliteral";
  html += String(upload.mqttPort);
  html += R"rawliteral("></label>)rawliteral";
  if (httpsUpload) {
    const String clientIdState = settings_->cloudflareAccessClientIdConfigured()
                                     ? "Configured — leave blank to keep"
                                     : "Not configured";
    const String clientSecretState = settings_->cloudflareAccessClientSecretConfigured()
                                         ? "Configured — leave blank to keep"
                                         : "Not configured";
    html += R"rawliteral(<h2>Cloudflare Access</h2><p class="hint">Enter either field only when replacing it. Existing values are write-only and cannot be retrieved from the logger.</p><label>Client ID<input name="cf_access_client_id" type="password" autocomplete="new-password" spellcheck="false" maxlength="127" placeholder=")rawliteral";
    html += htmlEscape(clientIdState);
    html += R"rawliteral("></label><label>Client secret<input name="cf_access_client_secret" type="password" autocomplete="new-password" spellcheck="false" maxlength="127" placeholder=")rawliteral";
    html += htmlEscape(clientSecretState);
    html += R"rawliteral("></label>)rawliteral";
  }
  html += R"rawliteral(<h2>Network time</h2><label>Primary NTP server<input name="ntp_primary" required maxlength="63" value=")rawliteral";
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
                       settings_->appliedConfigVersion(),
                       server_.arg("cf_access_client_id"),
                       server_.arg("cf_access_client_secret"))) {
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
  json += "\"remote_management_enabled\":" +
          String(state_.system.remoteManagementEnabled ? "true" : "false") + ",";
  json += "\"applied_config_version\":" +
          String(state_.system.appliedConfigVersion) + ",";
  json += "\"remote_management_status\":\"" +
          jsonEscape(state_.system.remoteManagementStatus) + "\",";
  json += "\"remote_management_error\":\"" +
          jsonEscape(state_.system.remoteManagementError) + "\",";
  json += "\"store_forward_enabled\":" +
          String(state_.system.storeForwardEnabled ? "true" : "false") + ",";
  json += "\"store_forward_ready\":" +
          String(state_.system.storeForwardReady ? "true" : "false") + ",";
  json += "\"store_forward_pending_records\":" +
          String(state_.system.storeForwardPendingRecords) + ",";
  json += "\"store_forward_pending_bytes\":" +
          String(state_.system.storeForwardPendingBytes) + ",";
  json += "\"store_forward_capacity_bytes\":" +
          String(state_.system.storeForwardCapacityBytes) + ",";
  json += "\"store_forward_dropped_records\":" +
          String(state_.system.storeForwardDroppedRecords) + ",";
  json += "\"store_forward_error\":\"" + jsonEscape(state_.system.storeForwardError) + "\",";
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
    :root { --bg:#09131f; --surface:#111d2a; --border:#29394a; --text:#ecf2f8; --muted:#95a8ba; --accent:#6dd6ff; --ok:#73d5a2; --warn:#f4c46c; --bad:#ff8d8d; }
    * { box-sizing: border-box; }
    body { margin: 0; font-family: "Segoe UI", system-ui, sans-serif; background: var(--bg); color: var(--text); }
    header { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; padding:18px 20px; background:#102235; border-bottom:1px solid var(--border); }
    h1 { margin: 0; font-size: 1.4rem; }
    .header-meta { margin-top:4px; color:var(--muted); font-size:.88rem; }
    .settings-link { flex:none; padding:9px 12px; border:1px solid var(--border); border-radius:8px; text-decoration:none; }
    .grid { display: grid; gap: 16px; padding: 16px; grid-template-columns: repeat(auto-fit, minmax(250px, 1fr)); }
    .card { min-width:0; background:var(--surface); border:1px solid var(--border); border-radius:12px; padding:16px; }
    .label { color:var(--muted); font-size:.78rem; font-weight:700; letter-spacing:.08em; text-transform:uppercase; }
    .value { font-size:2.2rem; margin-top:6px; }
    .status { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; margin-top:11px; font-size:.94rem; }
    .status > :last-child { min-width:0; text-align:right; }
    .detail-row { display:block; padding-top:11px; border-top:1px solid rgba(149,168,186,.16); }
    .detail-row span { display:block; }
    .detail-row span:last-child { margin-top:4px; color:var(--text); text-align:left; overflow-wrap:anywhere; }
    .state { font-size:.78rem; font-weight:700; letter-spacing:.04em; }
    .state.ok { color:var(--ok); }
    .state.warn { color:var(--warn); }
    .state.bad { color:var(--bad); }
    .diagnostic { color:var(--muted); overflow-wrap:anywhere; }
    ul { margin-bottom:0; padding-left:18px; }
    li + li { margin-top:8px; }
    a { color:var(--accent); }
    a:focus-visible { outline:2px solid var(--accent); outline-offset:3px; }
    @media (max-width:560px) { header { padding:16px; } .grid { padding:12px; gap:12px; grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <header>
    <div><h1>Motorsport Sensor Logger</h1><div class="header-meta" id="stamp">Waiting for data...</div></div>
    <a class="settings-link" href="/settings">Settings</a>
  </header>
  <section class="grid">
)rawliteral";

  const String htmlEnd = R"rawliteral(
    <div class="card">
      <div class="label">Connectivity</div>
      <div class="status"><span>Server</span><span class="state" id="uploadStatus">--</span></div>
      <div class="status"><span>Protocol</span><span id="uploadProtocol">--</span></div>
      <div class="status"><span>Wi-Fi</span><span id="wifiStatus">--</span></div>
      <div class="status"><span>Remote management</span><span class="state" id="remoteManagementStatus">--</span></div>
      <div class="status"><span>Applied configuration</span><span id="configVersion">--</span></div>
      <div class="status detail-row"><span>Upstream endpoint</span><span id="uploadServer">--</span></div>
    </div>
    <div class="card">
      <div class="label">Hardware</div>
      <div class="status"><span>ADC</span><span class="state" id="adcStatus">--</span></div>
      <div class="status"><span>RTC</span><span class="state" id="rtcStatus">--</span></div>
      <div class="status"><span>Last time sync</span><span id="rtcLastSync">--</span></div>
      <div class="status"><span>OTA updates</span><span class="state" id="otaStatus">--</span></div>
    </div>
    <div class="card">
      <div class="label">Storage</div>
      <div class="status"><span>Onboard queue</span><span id="queueStatus">--</span></div>
      <div class="status"><span>Dropped records</span><span id="queueDropped">--</span></div>
      <div class="status"><span>SD logging</span><span class="state" id="sdStatus">--</span></div>
      <div class="status detail-row"><span>Current log file</span><span id="logFile">--</span></div>
    </div>
    <div class="card">
      <div class="label">Diagnostics</div>
      <div class="status detail-row"><span>Upload session</span><span class="diagnostic" id="uploadSession">--</span></div>
      <div class="status"><span>Upload sequence</span><span id="uploadSequence">--</span></div>
      <div class="status detail-row"><span>Upload</span><span class="diagnostic" id="uploadError">No errors</span></div>
      <div class="status detail-row"><span>Remote management</span><span class="diagnostic" id="remoteManagementError">No errors</span></div>
      <div class="status detail-row"><span>Queue</span><span class="diagnostic" id="queueError">No errors</span></div>
      <div class="status detail-row"><span>RTC</span><span class="diagnostic" id="rtcError">No errors</span></div>
      <div class="status detail-row"><span>Logging</span><span class="diagnostic" id="logError">No errors</span></div>
    </div>
    <div class="card">
      <div class="label">CSV Files</div>
      <ul id="fileList"></ul>
    </div>
  </section>
  <script>
    function setState(id, text, tone) {
      const target = document.getElementById(id);
      target.textContent = text;
      target.className = 'state' + (tone ? ' ' + tone : '');
    }

    async function refreshLive() {
      const response = await fetch('/api/live');
      const data = await response.json();
      document.getElementById('stamp').textContent = data.timestamp + ' ' + data.system.time_zone + ' | uptime ' + data.uptime;
      data.sensors.forEach((sensor) => {
        document.getElementById('sensor-value-' + sensor.id).textContent = sensor.value.toFixed(1) + ' ' + sensor.units;
        document.getElementById('sensor-loop-' + sensor.id).textContent = sensor.loop_mA.toFixed(2) + ' mA';
        document.getElementById('sensor-fault-' + sensor.id).textContent = sensor.fault;
      });
      setState('adcStatus', data.system.adc_ready ? 'READY' : 'FAULT', data.system.adc_ready ? 'ok' : 'bad');
      const rtcText = data.system.rtc_enabled ? (data.system.rtc_ready ? (data.system.rtc_synced ? 'NTP SYNCED' : 'HOLDOVER') : 'FAULT') : 'DISABLED';
      setState('rtcStatus', rtcText, rtcText === 'FAULT' ? 'bad' : (rtcText === 'NTP SYNCED' ? 'ok' : 'warn'));
      document.getElementById('rtcLastSync').textContent = data.system.rtc_last_sync || '--';
      const sdText = data.system.sd_enabled ? (data.system.sd_ready ? 'READY' : 'FAULT') : 'DISABLED';
      setState('sdStatus', sdText, sdText === 'FAULT' ? 'bad' : (sdText === 'READY' ? 'ok' : ''));
      const uploadText = data.system.upload_enabled ? (data.system.upload_connected ? 'CONNECTED' : 'WAITING') : 'DISABLED';
      setState('uploadStatus', uploadText, uploadText === 'CONNECTED' ? 'ok' : (uploadText === 'WAITING' ? 'warn' : ''));
      document.getElementById('uploadProtocol').textContent = data.system.upload_protocol.toUpperCase();
      document.getElementById('uploadServer').textContent = data.system.upload_server || 'Not configured';
      const remoteText = data.system.remote_management_enabled ? 'ENABLED' : 'DISABLED';
      setState('remoteManagementStatus', remoteText, data.system.remote_management_enabled ? 'ok' : '');
      document.getElementById('configVersion').textContent = 'v' + data.system.applied_config_version + ' ' + (data.system.remote_management_status || 'ready');
      document.getElementById('queueStatus').textContent = data.system.store_forward_enabled
        ? (data.system.store_forward_ready
          ? data.system.store_forward_pending_records + ' pending / ' + Math.round(data.system.store_forward_pending_bytes / 1024) + ' KiB'
          : 'FAULT')
        : 'DISABLED';
      document.getElementById('queueDropped').textContent = data.system.store_forward_dropped_records;
      const otaText = data.system.ota_enabled ? (data.system.ota_ready ? 'READY' : 'LOCKED') : 'DISABLED';
      setState('otaStatus', otaText, otaText === 'READY' ? 'ok' : (otaText === 'LOCKED' ? 'warn' : ''));
      document.getElementById('wifiStatus').textContent = data.system.wifi_mode + ' ' + data.system.ip_address;
      document.getElementById('logFile').textContent = data.system.current_log_file || '--';
      document.getElementById('uploadSession').textContent = data.system.upload_session_id || '--';
      document.getElementById('uploadSequence').textContent = data.system.upload_sequence;
      document.getElementById('uploadError').textContent = data.system.last_upload_error || 'No errors';
      document.getElementById('remoteManagementError').textContent = data.system.remote_management_error || 'No errors';
      document.getElementById('queueError').textContent = data.system.store_forward_error || 'No errors';
      document.getElementById('rtcError').textContent = data.system.rtc_error || 'No errors';
      document.getElementById('logError').textContent = data.system.last_log_error || 'No errors';
    }

    async function refreshFiles() {
      const response = await fetch('/api/files');
      const files = await response.json();
      const target = document.getElementById('fileList');
      target.innerHTML = '';
      if (!files.length) {
        const empty = document.createElement('li');
        empty.textContent = 'No files available';
        target.appendChild(empty);
        return;
      }
      files.forEach((file) => {
        const li = document.createElement('li');
        const a = document.createElement('a');
        a.href = '/download/' + file.name.replace(/^\//, '');
        a.textContent = file.name + ' (' + file.size + ' B)';
        li.appendChild(a);
        target.appendChild(li);
      });
    }

    async function refreshLiveSafely() {
      try {
        await refreshLive();
      } catch (error) {
        document.getElementById('stamp').textContent = 'Web UI refresh failed';
      }
    }

    refreshLiveSafely();
    refreshFiles().catch(() => {});
    setInterval(refreshLiveSafely, 1000);
    setInterval(() => refreshFiles().catch(() => {}), 10000);
  </script>
</body>
</html>
)rawliteral";

  return htmlStart + sensorCardsHtml() + htmlEnd;
}
