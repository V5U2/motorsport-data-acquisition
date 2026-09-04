#include "WebUi.h"

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

bool WebUi::begin(const AppConfig::WifiConfig &config,
                  CsvLogger &logger,
                  RuntimeSettings &settings,
                  const char *deviceHostname,
                  const char *settingsPassword,
                  const bool localSettingsEnabled) {
  logger_ = &logger;
  settings_ = &settings;
  deviceHostname_ = deviceHostname;
  settingsPassword_ = settingsPassword;
  localSettingsEnabled_ = localSettingsEnabled;

  if (config.mode == AppConfig::WifiMode::Station && strlen(config.stationSsid) > 0) {
    WiFi.persistent(false);
#if defined(ESP8266)
    WiFi.hostname(deviceHostname_.c_str());
#else
    WiFi.setHostname(deviceHostname_.c_str());
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

  if (!ready_ && !config.fallbackApEnabled) {
    mode_ = "OFF";
    ipAddress_ = "0.0.0.0";
    return false;
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
  if (!ready_) {
    return;
  }
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
  server_.on("/diagnostics", HTTP_GET, [this]() { handleDiagnostics(); });
  server_.on("/api/live", HTTP_GET, [this]() { handleLiveJson(); });
  server_.on("/api/files", HTTP_GET, [this]() { handleFilesJson(); });
  server_.on("/settings", HTTP_GET, [this]() { handleSettings(); });
  server_.on("/settings", HTTP_POST, [this]() { handleSettingsSave(); });
  server_.onNotFound([this]() { handleDownload(); });
}

void WebUi::handleIndex() { server_.send(200, "text/html", indexHtml()); }

void WebUi::handleDiagnostics() {
  server_.send(200, "text/html", diagnosticsHtml());
}

void WebUi::handleLiveJson() { server_.send(200, "application/json", liveJson()); }

void WebUi::handleFilesJson() {
  if (logger_ == nullptr) {
    server_.send(503, "application/json", "[]");
    return;
  }
  server_.send(200, "application/json", logger_->listFilesJson());
}

void WebUi::handleSettings() {
  if (!localSettingsEnabled_) {
    server_.send(410, "text/plain",
                 "Local settings are disabled on ESP32. Use identity-bound USB provisioning or optional app management.");
    return;
  }
  if (!settingsAuthorized()) {
    return;
  }

  const AppConfig::UploadConfig &upload = settings_->uploadConfig();
  const bool httpsUpload = upload.protocol == AppConfig::UploadConfig::Protocol::Https;
  String html = R"rawliteral(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><link rel="icon" href="data:,"><title>Logger Settings</title><style>body{font-family:system-ui;max-width:38rem;margin:2rem auto;padding:0 1rem;background:#09131f;color:#ecf2f8}label{display:block;margin:1rem 0}.hint{color:#95a8ba}input{box-sizing:border-box;width:100%;padding:.7rem;margin-top:.3rem}input[type=checkbox]{width:auto}button{padding:.8rem 1.2rem}a{color:#6dd6ff}h2{margin-top:2rem}</style></head><body><h1>Device settings</h1><form method="post" action="/settings"><h2>Upstream server</h2>)rawliteral";
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
  if (!localSettingsEnabled_) {
    server_.send(410, "text/plain", "Local settings are disabled on ESP32");
    return;
  }
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
  if (settingsPassword_.isEmpty()) {
    server_.send(503, "text/plain", "Provision a device-specific settings credential over USB first");
    return false;
  }
  if (!server_.authenticate("admin", settingsPassword_.c_str())) {
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
  json += "\"device_id\":\"" + jsonEscape(state_.system.deviceId) + "\",";
  json += "\"device_name\":\"" + jsonEscape(state_.system.deviceName) + "\",";
  json += "\"hardware_revision\":\"" + jsonEscape(state_.system.hardwareRevision) + "\",";
  json += "\"provisioning_status\":\"" + jsonEscape(state_.system.provisioningStatus) + "\",";
  json += "\"provisioning_error\":\"" + jsonEscape(state_.system.provisioningError) + "\",";
  json += "\"provisioned_at\":\"" + jsonEscape(state_.system.provisionedAt) + "\",";
  json += "\"production_security_required\":" +
          String(state_.system.productionSecurityRequired ? "true" : "false") + ",";
  json += "\"secure_boot_enabled\":" +
          String(state_.system.secureBootEnabled ? "true" : "false") + ",";
  json += "\"flash_encryption_enabled\":" +
          String(state_.system.flashEncryptionEnabled ? "true" : "false") + ",";
  json += "\"flash_encryption_release_mode\":" +
          String(state_.system.flashEncryptionReleaseMode ? "true" : "false") + ",";
  json += "\"production_security_ready\":" +
          String(state_.system.productionSecurityReady ? "true" : "false") + ",";
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
  json += "\"store_forward_corruption_events\":" +
          String(state_.system.storeForwardCorruptionEvents) + ",";
  json += "\"store_forward_quarantined_bytes\":" +
          String(state_.system.storeForwardQuarantinedBytes) + ",";
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
  <link rel="icon" href="data:,">
  <title>Motorsport Logger</title>
  <style>
    :root { --bg:#09131f; --surface:#111d2a; --border:#29394a; --text:#ecf2f8; --muted:#95a8ba; --accent:#6dd6ff; --ok:#73d5a2; --warn:#f4c46c; --bad:#ff8d8d; }
    * { box-sizing: border-box; }
    body { margin: 0; font-family: "Segoe UI", system-ui, sans-serif; background: var(--bg); color: var(--text); }
    header { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; padding:18px 20px; background:#102235; border-bottom:1px solid var(--border); }
    h1 { margin: 0; font-size: 1.4rem; }
    .header-meta { margin-top:4px; color:var(--muted); font-size:.88rem; }
    .actions { display:flex; gap:8px; }
    .action-link { flex:none; padding:9px 12px; border:1px solid var(--border); border-radius:8px; color:var(--text); text-decoration:none; }
    main { width:min(100%, 72rem); margin:0 auto; padding:16px; }
    .sensor-grid { display:grid; gap:12px; grid-template-columns:repeat(auto-fit, minmax(180px, 240px)); }
    .support-grid { display:grid; gap:12px; margin-top:12px; grid-template-columns:repeat(2, minmax(0, 1fr)); }
    .card { min-width:0; background:var(--surface); border:1px solid var(--border); border-radius:12px; padding:14px; }
    .sensor-card { min-height:116px; }
    .label { color:var(--muted); font-size:.78rem; font-weight:700; letter-spacing:.08em; text-transform:uppercase; }
    .value { font-size:1.8rem; margin-top:6px; }
    .status { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; margin-top:11px; font-size:.94rem; }
    .status > :last-child { min-width:0; text-align:right; }
    .state { font-size:.78rem; font-weight:700; letter-spacing:.04em; }
    .state.ok { color:var(--ok); }
    .state.warn { color:var(--warn); }
    .state.bad { color:var(--bad); }
    .summary { margin:10px 0 0; color:var(--muted); line-height:1.45; }
    .card-link { display:inline-block; margin-top:12px; }
    .disabled { opacity:.52; }
    .disabled .card-link { color:var(--muted); pointer-events:none; text-decoration:none; }
    ul { margin:10px 0 0; padding-left:18px; }
    li + li { margin-top:8px; }
    a { color:var(--accent); }
    a:focus-visible { outline:2px solid var(--accent); outline-offset:3px; }
    @media (max-width:620px) { header { padding:16px; flex-direction:column; } .actions { flex-direction:row; } main { padding:12px; } .sensor-grid,.support-grid { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <header>
    <div><h1>Motorsport Sensor Logger</h1><div class="header-meta" id="stamp">Waiting for data...</div></div>
    <nav class="actions" aria-label="Logger pages"><a class="action-link" href="/diagnostics">Diagnostics</a><a class="action-link" href="/settings">Settings</a></nav>
  </header>
  <main>
    <section class="sensor-grid" aria-label="Sensor readings">
)rawliteral";

  const String htmlEnd = R"rawliteral(
    </section>
    <section class="support-grid" aria-label="Logger status">
      <div class="card" id="healthCard">
        <div class="label">Fault finding</div>
        <div class="status"><span>Logger health</span><span class="state" id="healthStatus">CHECKING</span></div>
        <p class="summary" id="healthSummary">Checking sensors and logger systems...</p>
        <a class="card-link" href="/diagnostics">View full diagnostics</a>
      </div>
      <div class="card" id="csvCard">
      <div class="label">CSV Files</div>
      <p class="summary" id="csvSummary">Checking microSD logging...</p>
      <ul id="fileList"></ul>
      </div>
    </section>
  </main>
  <script>
    let csvFilesEnabled = false;

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
        const fault = document.getElementById('sensor-fault-' + sensor.id);
        fault.textContent = sensor.fault === 'none' ? 'OK' : sensor.fault.toUpperCase();
        fault.className = 'state ' + (sensor.fault === 'none' ? 'ok' : 'bad');
      });
      const issues = [];
      data.sensors.forEach((sensor) => { if (sensor.fault !== 'none') issues.push(sensor.name + ': ' + sensor.fault); });
      if (!data.system.adc_ready) issues.push('ADC is not ready');
      if (data.system.rtc_enabled && !data.system.rtc_ready) issues.push('RTC: ' + (data.system.rtc_error || 'not ready'));
      else if (data.system.rtc_enabled && data.system.rtc_error) issues.push('RTC: ' + data.system.rtc_error);
      if (data.system.sd_enabled && !data.system.sd_ready) issues.push('Logging: ' + (data.system.last_log_error || 'microSD is not ready'));
      else if (data.system.sd_enabled && data.system.last_log_error) issues.push('Logging: ' + data.system.last_log_error);
      if (data.system.upload_enabled && !data.system.upload_connected) issues.push('Upload: ' + (data.system.last_upload_error || 'upstream server is not connected'));
      else if (data.system.last_upload_error) issues.push('Upload: ' + data.system.last_upload_error);
      if (data.system.remote_management_error) issues.push('Remote management: ' + data.system.remote_management_error);
      if (data.system.store_forward_enabled && !data.system.store_forward_ready) issues.push('Queue: ' + (data.system.store_forward_error || 'not ready'));
      else if (data.system.store_forward_error) issues.push('Queue: ' + data.system.store_forward_error);
      if (issues.length) {
        setState('healthStatus', issues.length + ' ISSUE' + (issues.length === 1 ? '' : 'S'), 'bad');
        document.getElementById('healthSummary').textContent = issues.slice(0, 2).join('. ') + (issues.length > 2 ? '. View diagnostics for more.' : '.');
      } else {
        setState('healthStatus', 'NOMINAL', 'ok');
        document.getElementById('healthSummary').textContent = 'Sensors and enabled logger systems are operating normally.';
      }
      const csvCard = document.getElementById('csvCard');
      const csvSummary = document.getElementById('csvSummary');
      if (!data.system.sd_enabled) {
        csvFilesEnabled = false;
        csvCard.classList.add('disabled');
        csvCard.setAttribute('aria-disabled', 'true');
        csvSummary.textContent = 'MicroSD logging is disabled in this firmware.';
        document.getElementById('fileList').innerHTML = '';
      } else if (!data.system.sd_ready) {
        csvFilesEnabled = false;
        csvCard.classList.remove('disabled');
        csvCard.removeAttribute('aria-disabled');
        csvSummary.textContent = 'MicroSD logging is enabled but the card is not ready.';
      } else {
        const shouldLoadFiles = !csvFilesEnabled;
        csvFilesEnabled = true;
        csvCard.classList.remove('disabled');
        csvCard.removeAttribute('aria-disabled');
        csvSummary.textContent = 'Download logs stored on the microSD card.';
        if (shouldLoadFiles) refreshFiles();
      }
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
    setInterval(refreshLiveSafely, 1000);
    setInterval(() => { if (csvFilesEnabled) refreshFiles().catch(() => {}); }, 10000);
  </script>
</body>
</html>
)rawliteral";

  return htmlStart + sensorCardsHtml() + htmlEnd;
}

String WebUi::diagnosticsHtml() const {
  return R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <title>Logger Diagnostics</title>
  <style>
    :root { --bg:#09131f; --surface:#111d2a; --border:#29394a; --text:#ecf2f8; --muted:#95a8ba; --accent:#6dd6ff; --ok:#73d5a2; --warn:#f4c46c; --bad:#ff8d8d; }
    * { box-sizing:border-box; }
    body { margin:0; font-family:"Segoe UI",system-ui,sans-serif; background:var(--bg); color:var(--text); }
    header { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; padding:18px 20px; background:#102235; border-bottom:1px solid var(--border); }
    h1 { margin:0; font-size:1.4rem; }
    .header-meta { margin-top:4px; color:var(--muted); font-size:.88rem; }
    .actions { display:flex; gap:8px; }
    .action-link { padding:9px 12px; border:1px solid var(--border); border-radius:8px; color:var(--text); text-decoration:none; }
    main { width:min(100%,72rem); margin:0 auto; padding:16px; }
    .grid { display:grid; gap:12px; grid-template-columns:repeat(2,minmax(0,1fr)); }
    .card { min-width:0; background:var(--surface); border:1px solid var(--border); border-radius:12px; padding:16px; }
    .label { color:var(--muted); font-size:.78rem; font-weight:700; letter-spacing:.08em; text-transform:uppercase; }
    .status { display:flex; align-items:flex-start; justify-content:space-between; gap:16px; margin-top:11px; font-size:.94rem; }
    .status > :last-child { min-width:0; text-align:right; overflow-wrap:anywhere; }
    .detail-row { display:block; padding-top:11px; border-top:1px solid rgba(149,168,186,.16); }
    .detail-row span { display:block; }
    .detail-row span:last-child { margin-top:4px; color:var(--text); text-align:left; }
    .state { font-size:.78rem; font-weight:700; letter-spacing:.04em; }
    .state.ok { color:var(--ok); } .state.warn { color:var(--warn); } .state.bad { color:var(--bad); }
    a { color:var(--accent); } a:focus-visible { outline:2px solid var(--accent); outline-offset:3px; }
    @media(max-width:620px) { header { padding:16px; flex-direction:column; } .actions { flex-direction:row; } main { padding:12px; } .grid { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <header><div><h1>Logger Diagnostics</h1><div class="header-meta" id="stamp">Waiting for data...</div></div><nav class="actions" aria-label="Logger pages"><a class="action-link" href="/">Dashboard</a><a class="action-link" href="/settings">Settings</a></nav></header>
  <main><section class="grid">
    <div class="card"><div class="label">Identity &amp; provisioning</div><div class="status detail-row"><span>Logger ID</span><span id="deviceId">--</span></div><div class="status detail-row"><span>Name</span><span id="deviceName">--</span></div><div class="status"><span>Provisioning</span><span class="state" id="provisioningStatus">--</span></div><div class="status"><span>Hardware revision</span><span id="hardwareRevision">--</span></div><div class="status"><span>Provisioned at</span><span id="provisionedAt">--</span></div><div class="status detail-row"><span>Provisioning error</span><span id="provisioningError">No errors</span></div></div>
    <div class="card"><div class="label">Connectivity</div><div class="status"><span>Server</span><span class="state" id="uploadStatus">--</span></div><div class="status"><span>Protocol</span><span id="uploadProtocol">--</span></div><div class="status"><span>Wi-Fi</span><span id="wifiStatus">--</span></div><div class="status"><span>Remote management</span><span class="state" id="remoteManagementStatus">--</span></div><div class="status"><span>Applied configuration</span><span id="configVersion">--</span></div><div class="status detail-row"><span>Upstream endpoint</span><span id="uploadServer">--</span></div></div>
    <div class="card"><div class="label">Hardware &amp; time</div><div class="status"><span>ADC</span><span class="state" id="adcStatus">--</span></div><div class="status"><span>RTC</span><span class="state" id="rtcStatus">--</span></div><div class="status"><span>Last time sync</span><span id="rtcLastSync">--</span></div><div class="status"><span>Secure boot</span><span class="state" id="secureBootStatus">--</span></div><div class="status"><span>Flash encryption</span><span class="state" id="flashEncryptionStatus">--</span></div><div class="status"><span>Production gate</span><span class="state" id="productionSecurityStatus">--</span></div><div class="status"><span>OTA updates</span><span class="state" id="otaStatus">--</span></div></div>
    <div class="card"><div class="label">Storage</div><div class="status"><span>Onboard queue</span><span id="queueStatus">--</span></div><div class="status"><span>Queue capacity</span><span id="queueCapacity">--</span></div><div class="status"><span>Dropped records</span><span id="queueDropped">--</span></div><div class="status"><span>Corruption repairs</span><span id="queueCorruption">--</span></div><div class="status"><span>Quarantined bytes</span><span id="queueQuarantined">--</span></div><div class="status"><span>SD logging</span><span class="state" id="sdStatus">--</span></div><div class="status detail-row"><span>Current log file</span><span id="logFile">--</span></div></div>
    <div class="card"><div class="label">Transport</div><div class="status detail-row"><span>Upload session</span><span id="uploadSession">--</span></div><div class="status"><span>Upload sequence</span><span id="uploadSequence">--</span></div><div class="status detail-row"><span>Upload error</span><span id="uploadError">No errors</span></div><div class="status detail-row"><span>Remote-management error</span><span id="remoteManagementError">No errors</span></div></div>
    <div class="card"><div class="label">Hardware errors</div><div class="status detail-row"><span>Queue</span><span id="queueError">No errors</span></div><div class="status detail-row"><span>RTC</span><span id="rtcError">No errors</span></div><div class="status detail-row"><span>Logging</span><span id="logError">No errors</span></div></div>
    <div class="card"><div class="label">Sensors</div><div id="sensorDiagnostics">--</div></div>
  </section></main>
  <script>
    function text(id,value){document.getElementById(id).textContent=value;}
    function state(id,value,tone){const el=document.getElementById(id);el.textContent=value;el.className='state'+(tone?' '+tone:'');}
    async function refresh(){
      const response=await fetch('/api/live'); const data=await response.json();
      text('stamp',data.timestamp+' '+data.system.time_zone+' | uptime '+data.uptime);
      text('deviceId',data.system.device_id||'--'); text('deviceName',data.system.device_name||'--'); text('hardwareRevision',data.system.hardware_revision||'--'); text('provisionedAt',data.system.provisioned_at||'--'); const provisioned=data.system.provisioning_status==='provisioned'; state('provisioningStatus',(data.system.provisioning_status||'unknown').toUpperCase(),provisioned?'ok':'warn'); text('provisioningError',data.system.provisioning_error||'No errors');
      const upload=data.system.upload_enabled?(data.system.upload_connected?'CONNECTED':'WAITING'):'DISABLED'; state('uploadStatus',upload,upload==='CONNECTED'?'ok':(upload==='WAITING'?'warn':''));
      text('uploadProtocol',data.system.upload_protocol.toUpperCase()); text('wifiStatus',data.system.wifi_mode+' '+data.system.ip_address); text('uploadServer',data.system.upload_server||'Not configured');
      state('remoteManagementStatus',data.system.remote_management_enabled?'ENABLED':'DISABLED',data.system.remote_management_enabled?'ok':''); text('configVersion','v'+data.system.applied_config_version+' '+(data.system.remote_management_status||'ready'));
      state('adcStatus',data.system.adc_ready?'READY':'FAULT',data.system.adc_ready?'ok':'bad');
      state('secureBootStatus',data.system.secure_boot_enabled?'ENABLED':'DISABLED',data.system.secure_boot_enabled?'ok':'warn'); const flashMode=data.system.flash_encryption_release_mode?'RELEASE':(data.system.flash_encryption_enabled?'DEVELOPMENT':'DISABLED'); state('flashEncryptionStatus',flashMode,data.system.flash_encryption_release_mode?'ok':'warn'); const productionReady=data.system.production_security_ready; state('productionSecurityStatus',data.system.production_security_required?(productionReady?'READY':'BLOCKED'):'DEVELOPMENT',data.system.production_security_required?(productionReady?'ok':'bad'):'warn');
      const rtc=data.system.rtc_enabled?(data.system.rtc_ready?(data.system.rtc_synced?'NTP SYNCED':'HOLDOVER'):'FAULT'):'DISABLED'; state('rtcStatus',rtc,rtc==='FAULT'?'bad':(rtc==='NTP SYNCED'?'ok':'warn')); text('rtcLastSync',data.system.rtc_last_sync||'--');
      const ota=data.system.ota_enabled?(data.system.ota_ready?'READY':'LOCKED'):'DISABLED'; state('otaStatus',ota,ota==='READY'?'ok':(ota==='LOCKED'?'warn':''));
      text('queueStatus',data.system.store_forward_enabled?(data.system.store_forward_ready?data.system.store_forward_pending_records+' pending / '+Math.round(data.system.store_forward_pending_bytes/1024)+' KiB':'FAULT'):'DISABLED'); text('queueCapacity',Math.round(data.system.store_forward_capacity_bytes/1024)+' KiB'); text('queueDropped',data.system.store_forward_dropped_records); text('queueCorruption',data.system.store_forward_corruption_events); text('queueQuarantined',data.system.store_forward_quarantined_bytes);
      const sd=data.system.sd_enabled?(data.system.sd_ready?'READY':'FAULT'):'DISABLED'; state('sdStatus',sd,sd==='READY'?'ok':(sd==='FAULT'?'bad':'')); text('logFile',data.system.current_log_file||'--');
      text('uploadSession',data.system.upload_session_id||'--'); text('uploadSequence',data.system.upload_sequence); text('uploadError',data.system.last_upload_error||'No errors'); text('remoteManagementError',data.system.remote_management_error||'No errors'); text('queueError',data.system.store_forward_error||'No errors'); text('rtcError',data.system.rtc_error||'No errors'); text('logError',data.system.last_log_error||'No errors');
      const sensors=document.getElementById('sensorDiagnostics'); sensors.innerHTML=''; data.sensors.forEach((sensor)=>{const row=document.createElement('div');row.className='status';const name=document.createElement('span');name.textContent=sensor.name+' ('+sensor.loop_mA.toFixed(2)+' mA)';const fault=document.createElement('span');fault.className='state '+(sensor.fault==='none'?'ok':'bad');fault.textContent=sensor.fault==='none'?'OK':sensor.fault.toUpperCase();row.append(name,fault);sensors.appendChild(row);});
    }
    async function refreshSafely(){try{await refresh();}catch(error){text('stamp','Diagnostics refresh failed');}}
    refreshSafely(); setInterval(refreshSafely,1000);
  </script>
</body></html>
)rawliteral";
}
