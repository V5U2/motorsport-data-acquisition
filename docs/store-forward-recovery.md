# ESP32 store-and-forward recovery

The onboard queue is an outage buffer for the 16 MiB ESP32 layout, not a replacement for removable SD logging. It is enabled only for HTTPS transport failures and uses at most 10 MiB of the LittleFS partition. ESP8266 and ESP32 layouts without enough LittleFS capacity report the queue as unavailable and continue without onboard replay.

## Durable format

The queue has two append-only segment files, `/sfq-0.log` and `/sfq-1.log`, plus `/sfq.meta`. Each record contains the `SFQ1` magic, a 32-bit payload length, a 32-bit FNV-1a checksum, and the exact JSON snapshot bytes. Payloads are limited to 4096 bytes. Metadata format version 2 contains the active segment, replay head for both segments, cumulative capacity drops, corruption repairs, quarantined bytes, and its own FNV-1a checksum. Version 1 metadata is migrated in place without rewriting segment records.

At startup, firmware scans every record from the committed head. A complete record with a wrong header, invalid length, truncated payload, or wrong checksum ends the valid prefix. The invalid suffix is copied to `/sfq-0.corrupt` or `/sfq-1.corrupt` before the live segment is repaired. The local diagnostics page and `/api/live` expose `store_forward_corruption_events`, `store_forward_quarantined_bytes`, and `store_forward_error`. Existing quarantine files are never replayed.

## Interruption semantics

| Interruption | Deterministic result after reboot |
| --- | --- |
| During record append | The last incomplete record is quarantined; the preceding valid records remain ordered and replayable. |
| During metadata replacement | Segment records are rescanned. A replay acknowledgement that was not durably committed may be sent again, giving at-least-once delivery. |
| After acknowledgement commit but before empty-segment reclamation | The committed head prevents replay; startup safely reclaims the empty segment. |
| During segment rotation | The previously committed active segment remains recoverable. Capacity drops are committed with the rotation metadata. |
| Mount failure | Firmware does not request a format. It reports a fault and leaves every file untouched. |
| Invalid server response during replay | Retryable failures remain queued. A permanently rejected record is explicitly popped and increments the drop counter. |

The ingest API must therefore remain idempotent for a device/session/sequence identity. At-least-once replay can repeat an acknowledged snapshot after an interrupted metadata commit, but it must not create duplicate final state. The managed HTTPS receiver derives a stable identity from kind, device, source session, sequence, and timestamp, then writes the same InfluxDB series/timestamp on every exact retry. A `202` response acknowledges durable TSDB acceptance; `408`, `425`, `429`, transport failures, and `5xx` remain queued, while permanent `400`, `404`, `413`, and `422` rejects are dropped and counted.

## Capacity and endurance

Usable record count is approximately `10 MiB / (JSON payload bytes + 12)`. Retention is that count divided by the configured HTTPS publish rate. At the current 250 ms interval, a 512-byte snapshot retains about 83 minutes; the maximum 4096-byte payload retains about 11 minutes. Actual commissioning must measure the emitted payload size for the configured sensor set. A multi-hour outage can exceed the buffer, at which point rotation deliberately removes the oldest whole segment and increments `store_forward_dropped_records`.

Only snapshots that fail retryably are written, so normal connected running causes no segment writes. During an outage, a 512-byte payload at 4 Hz writes about 7.5 MiB per hour before filesystem overhead. LittleFS wear levelling and the flash vendor's erase-cycle rating determine service life; do not use the queue as continuous primary storage. Use SD logging when the required outage retention exceeds the measured buffer or when long-term archives are required.

Replay appends the current sample first and then attempts up to two oldest queued records, so a healthy connection drains at least one net record per publish cycle while preserving order. HTTPS calls are currently synchronous; physical commissioning must confirm the configured HTTP timeout does not violate sampling or watchdog requirements before this behavior is approved for track use.

## Recovery procedure

1. Record the queue counters and error shown on **Diagnostics**, and save `/api/live` if incident evidence is required.
2. Reboot once. Startup repair is non-destructive to the valid prefix and preserves an invalid suffix in the `.corrupt` file.
3. If LittleFS still will not mount, stop. Do not deploy firmware that calls `LittleFS.begin(true, ...)` and do not erase flash until the queued partition has been captured or explicitly declared expendable.
4. After evidence is captured and loss is accepted, perform an intentional full-device erase with `pio run -t erase`, then reinstall the known-good firmware and settings. This destroys settings, queue data, and quarantine evidence.
5. Confirm `store_forward_ready=true`, zero pending records, and an empty queue error before returning the logger to service.

Physical release evidence must include one interrupted append or metadata commit, a reconnect replay, observed queue counters, and confirmation that sensor sampling and the watchdog remain healthy.
