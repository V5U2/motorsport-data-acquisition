#include "LiveUpload.h"
#include "Logic.h"
#include "QueueAge.h"
#include <time.h>

#if defined(ESP32)
#include <WiFi.h>
#include <cstdio>

bool LiveUpload::captureHttps(const AppState &state) {
  const uint32_t nowMs = millis();
  if (!Logic::intervalElapsed(nowMs, lastPublishMs_, config_.publishIntervalMs)) return false;
  lastPublishMs_ = nowMs;
  // Allocate once at capture, never at retry. Persist before submitting so
  // newer captures cannot overtake a failed request while the worker is busy.
  const String payload = buildSnapshotJson(state, ++lastSequence_);
  if (storeForwardQueue_.isReady()) {
    if (!storeForwardQueue_.enqueue(payload)) {
      captureDrops_ = StatusDiagnostics::saturatingAdd(captureDrops_);
      lastError_ = "Snapshot capture failed: " + storeForwardQueue_.lastError();
      return false;
    }
    refreshQueueOldest();
    return true;
  }
  // Queue-disabled/unavailable boards have one pending RAM slot in addition
  // to the worker's in-flight record. Never grow RAM with outage duration.
  if (!volatileSnapshot_.isEmpty()) {
    captureDrops_ = StatusDiagnostics::saturatingAdd(captureDrops_);
    lastError_ = "Upload RAM slot full; snapshot dropped (no durable queue)";
    return false;
  }
  volatileSnapshot_ = payload;
  return true;
}

bool LiveUpload::submitHttps(const HttpsOperation operation, const String &payload,
                             const char *bearer) {
  String url = "https://" + String(config_.mqttHost);
  if (config_.mqttPort != 443) url += ":" + String(config_.mqttPort);
  url += String(config_.httpsPath) + (operation == HttpsOperation::Snapshot ? "/snapshot" : "/status");
  if (bearer == nullptr) bearer = bearerRotation_ == nullptr ? config_.appDeviceToken
                                                           : bearerRotation_->activeBearer();
  if (!httpsWorker_.submit(url.c_str(), payload.c_str(), bearer,
                           config_.cloudflareAccessClientId, config_.cloudflareAccessClientSecret)) {
    lastError_ = "HTTPS worker unavailable or request exceeds bounded capacity";
    httpsConnected_ = false;
    httpsBackoff_ = true;
    lastHttpsAttemptMs_ = millis();
    return false;
  }
  diagnostics_.transportAttempt(httpsRecoveryPending_);
  httpsOperation_ = operation;
  return true;
}

void LiveUpload::serviceHttps(const uint32_t nowMs) {
  if (httpsOperation_ != HttpsOperation::None) {
    if (httpsWorker_.result() == nullptr) return;  // No blocking wait.
    completeHttps(nowMs);
    return;  // At most one completion or submission per loop iteration.
  }
  if (WiFi.status() != WL_CONNECTED) {
    publishOfflineStatusAndDisconnect();
    lastError_ = "Wi-Fi disconnected";
    return;
  }
  const auto work = httpsPacing_.next(nowMs, false, true, httpsBackoff_, lastHttpsAttemptMs_,
                                    config_.reconnectIntervalMs, httpsStatusRequested_,
                                    storeForwardQueue_.pendingRecords() > 0 || !volatileSnapshot_.isEmpty());
  if (work == HttpsPacing::Work::Wait) return;
  if (work == HttpsPacing::Work::Status) {
    HttpsOperation operation = HttpsOperation::Status;
    const char *bearer = nullptr;
    if (httpsFallbackRequested_) {
      operation = HttpsOperation::RotationFallback;
    } else if (bearerRotation_ != nullptr && bearerRotation_->hasCandidate()) {
      if (bearerRotation_->phase() == AppBearerRotation::Phase::Staged) {
        operation = HttpsOperation::RotationAck;
      } else {
        operation = HttpsOperation::RotationProof;
        bearer = bearerRotation_->candidateBearer();
      }
    }
    if (submitHttps(operation, buildStatusJson(true), bearer)) {
      httpsStatusRequested_ = false;
      httpsFallbackRequested_ = false;
    } else {
      // Local status size/allocation failure also yields snapshot work.
      httpsStatusRequested_ = false;
      httpsFallbackRequested_ = false;
      httpsPacing_.statusCompleted(nowMs);
    }
    return;
  }
  String payload;
  const bool durable = storeForwardQueue_.isReady() && storeForwardQueue_.pendingRecords() > 0;
  if (durable) {
    if (!storeForwardQueue_.peek(payload)) {
      lastError_ = "Onboard queue read failed: " + storeForwardQueue_.lastError();
      httpsBackoff_ = true;
      lastHttpsAttemptMs_ = nowMs;
      return;
    }
  } else {
    payload = volatileSnapshot_;
  }
  if (payload.isEmpty()) return;
  if (submitHttps(HttpsOperation::Snapshot, payload, nullptr)) {
    httpsSnapshotPayload_ = payload;
    httpsSnapshotDurable_ = durable;
    if (!durable) volatileSnapshot_ = "";
  }
}

void LiveUpload::completeHttps(const uint32_t nowMs) {
  const auto *result = httpsWorker_.result();
  const int status = result->status;
  const String response(result->body);
  const HttpsOperation operation = httpsOperation_;
  httpsWorker_.release();
  httpsOperation_ = HttpsOperation::None;
  lastHttpStatus_ = status;
  lastHttpsAttemptMs_ = nowMs;  // Backoff starts at completion, not submission.
  const bool accepted = status >= 200 && status < 300;
  if (operation != HttpsOperation::Snapshot) httpsPacing_.statusCompleted(nowMs);
  lastPostRetryable_ = Logic::isRetryableHttpStatus(status);
  httpsBackoff_ = !accepted;
  httpsRecoveryPending_ = !accepted;
  httpsConnected_ = accepted && WiFi.status() == WL_CONNECTED;
  lastError_ = accepted ? "" : "HTTPS upload failed " + String(status);

  if (operation == HttpsOperation::Snapshot) {
    const bool discard = !accepted && Logic::shouldDiscardQueuedHttpStatus(status);
    if (httpsSnapshotDurable_) {
      if ((accepted || discard) && !storeForwardQueue_.popIfMatches(httpsSnapshotPayload_, discard)) {
        lastError_ = "Onboard queue acknowledge failed: " + storeForwardQueue_.lastError();
        httpsBackoff_ = true;
      }
      refreshQueueOldest();
    } else if (!accepted) {
      // Without a filesystem keep the failed older record, sacrificing the
      // newer RAM slot if necessary. Permanent rejection is a counted drop.
      if (discard) {
        captureDrops_ = StatusDiagnostics::saturatingAdd(captureDrops_);
      } else {
        if (!volatileSnapshot_.isEmpty()) captureDrops_ = StatusDiagnostics::saturatingAdd(captureDrops_);
        volatileSnapshot_ = httpsSnapshotPayload_;
      }
    }
    httpsSnapshotPayload_ = "";
    return;
  }

  if (operation == HttpsOperation::RotationAck && bearerRotation_ != nullptr) {
    if (accepted) {
      if (!bearerRotation_->markAcknowledged()) {
        managementStatus_ = "rejected";
        managementError_ = bearerRotation_->lastError();
        httpsBackoff_ = true;
        return;
      }
    } else if (status >= 400 && status < 500 && !lastPostRetryable_) {
      bearerRotation_->abandonCandidate();
      managementStatus_ = "rejected";
      managementError_ = "Credential rotation acknowledgement rejected; candidate discarded";
      httpsStatusRequested_ = true;
      httpsFallbackRequested_ = true;
    }
  } else if (operation == HttpsOperation::RotationProof && bearerRotation_ != nullptr) {
    if (accepted) {
      if (!bearerRotation_->promoteCandidate()) {
        managementStatus_ = "pending";
        managementError_ = bearerRotation_->lastError();
        httpsBackoff_ = true;
        return;
      }
      managementStatus_ = "applied";
      managementError_ = "";
    } else {
      httpsStatusRequested_ = true;
      httpsFallbackRequested_ = true;
      managementStatus_ = "pending";
      managementError_ = "New app bearer not accepted; old bearer retained";
    }
  }
  if (accepted) {
    lastStatusPublishMs_ = nowMs;
    StaticJsonDocument<96> filter;
    filter["accepted"] = true;
    filter["status"] = true;
    StaticJsonDocument<192> acknowledgement;
    if (deserializeJson(acknowledgement, response, DeserializationOption::Filter(filter)) == DeserializationError::Ok &&
        acknowledgement["accepted"].is<bool>() && acknowledgement["accepted"].as<bool>() &&
        String(acknowledgement["status"] | "") == "ok") {
      authenticatedHeartbeatObserved_ = true;
      lastAuthenticatedHeartbeatMs_ = nowMs;
    }
    consumeHttpsDesiredConfig(response);
  }
}

void LiveUpload::refreshQueueOldest() {
  queueOldestSession_ = "";
  queueOldestTimestamp_ = "";
  queueOldestSequence_ = 0;
  queueOldestEpoch_ = 0;
  String payload;
  if (!storeForwardQueue_.isReady() || !storeForwardQueue_.peek(payload)) return;
  StaticJsonDocument<192> filter;
  filter["session_id"] = true;
  filter["sequence"] = true;
  filter["timestamp"] = true;
  StaticJsonDocument<512> metadata;
  if (deserializeJson(metadata, payload, DeserializationOption::Filter(filter)) != DeserializationError::Ok ||
      !metadata["session_id"].is<const char *>() || !metadata["sequence"].is<uint32_t>() ||
      !metadata["timestamp"].is<const char *>()) return;
  const char *session = metadata["session_id"];
  const char *timestamp = metadata["timestamp"];
  if (strlen(session) == 0 || strlen(session) > 128 ||
      Logic::normalizeTopicSegment(session) != session || strlen(timestamp) > 32) return;
  queueOldestSession_ = session;
  queueOldestSequence_ = metadata["sequence"];
  queueOldestTimestamp_ = timestamp;
  // Transport timestamps are UTC. Never interpret the uptime fallback or
  // malformed dates as wall time, and never apply the user's local timezone.
  queueOldestEpoch_ = QueueAge::epoch(timestamp);
}
#endif

String LiveUpload::queueOldestDiagnostics() const {
#if defined(ESP32)
  if (queueOldestSession_.isEmpty()) return "null";
  String json = "{\"session_id\":\"" + jsonEscape(queueOldestSession_) + "\",\"sequence\":" +
                String(queueOldestSequence_) + ",\"timestamp\":\"" + jsonEscape(queueOldestTimestamp_) + "\",\"age_seconds\":";
  const time_t now = time(nullptr);
  uint32_t age = 0;
  json += QueueAge::age(queueOldestEpoch_, now, age) ? String(age) : "null";
  return json + "}";
#else
  return "null";
#endif
}

uint32_t LiveUpload::uploadCaptureDrops() const {
#if defined(ESP32)
  return captureDrops_;
#else
  return 0;
#endif
}

bool LiveUpload::hasAuthenticatedHeartbeat() const {
  return config_.protocol == AppConfig::UploadConfig::Protocol::Https &&
         authenticatedHeartbeatObserved_ && httpsConnected_ &&
         uint32_t(millis() - lastAuthenticatedHeartbeatMs_) < 60000;
}
