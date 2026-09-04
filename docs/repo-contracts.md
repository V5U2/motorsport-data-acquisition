# Repo Contracts

This repo uses a small set of source-of-truth files for the important facts, and scripts enforce the automated checks around them.

## Source Ownership

| Concern | Owning file(s) | Notes |
| --- | --- | --- |
| PlatformIO environment and board target | [`platformio.ini`](../platformio.ini) | Canonical source for build env name, board ID, and build flags |
| Pin map | [`include/PinDefinitions.h`](../include/PinDefinitions.h) | Canonical source for NodeMCU D-label/GPIO assignments |
| Display wiring handoff | [`include/TFT_Setup.h`](../include/TFT_Setup.h) | Must consume the pin macros from `PinDefinitions.h` |
| Feature toggles, RTC selection, sensor config, MQTT/HTTPS upload and OTA config | [`include/AppConfig.h`](../include/AppConfig.h) | Canonical source for firmware configuration defaults |
| Persistent device settings | [`include/RuntimeSettings.h`](../include/RuntimeSettings.h), [`src/RuntimeSettings.cpp`](../src/RuntimeSettings.cpp) | Checksummed flash-backed upstream endpoint, upload enable flag, NTP servers, and timezone settings |
| Immutable identity and owner provisioning | [`include/DeviceProvisioning.h`](../include/DeviceProvisioning.h), [`src/DeviceProvisioning.cpp`](../src/DeviceProvisioning.cpp), [`include/ProvisioningPolicy.h`](../include/ProvisioningPolicy.h), [`src/ProvisioningPolicy.cpp`](../src/ProvisioningPolicy.cpp), [`docs/provisioning.md`](provisioning.md) | ESP32 eFuse identity, NVS ownership boundary, USB provisioning/factory-reset policy and operator runbook |
| Onboard store-and-forward | [`include/StoreForwardQueue.h`](../include/StoreForwardQueue.h), [`src/StoreForwardQueue.cpp`](../src/StoreForwardQueue.cpp), [`partitions/esp32-16mb-store-forward.csv`](../partitions/esp32-16mb-store-forward.csv), [`docs/store-forward-recovery.md`](store-forward-recovery.md) | ESP32 LittleFS queue format, rotation/recovery policy, capacity, and flash partition ownership |
| Live transport payload schema version | [`include/Logic.h`](../include/Logic.h) | `Logic::kLivePayloadSchemaVersion` is the canonical version emitted in MQTT and HTTPS live/status payloads |
| Wiring, detailed BOM, commissioning guidance, pin table | [`docs/hardware-setup.md`](./hardware-setup.md) | Human-oriented hardware source of truth |
| Project overview, build entrypoints, verification entrypoints | [`README.md`](../README.md) | Summary only; should link to owning docs instead of duplicating them |
| Agent workflow expectations | [`AGENTS.md`](../AGENTS.md) | Operational guidance and pointers, not a second source of hardware truth |
| Host-test entrypoint | [`scripts/run-host-tests.sh`](../scripts/run-host-tests.sh) | Canonical fast logic-test command |
| Repo-wide verification wrapper | [`scripts/verify-repo.sh`](../scripts/verify-repo.sh) | Canonical local/CI verification entrypoint |
| CI build/release behavior | [`.github/workflows/`](../.github/workflows/) | Must stay aligned with `platformio.ini` env/artifact paths |

## Verification Contract

### Fast path

Use for quick local checks and CI contract validation:

```sh
./scripts/verify-repo.sh --fast
```

This must run:
- host logic tests
- repo contract checks

### Full path

Use when the local toolchain is available or in firmware-oriented CI lanes:

```sh
./scripts/verify-repo.sh --full
```

This must run:
- the fast path
- the repo-local PlatformIO firmware build

## Change Rules

- If the board target changes, update [`platformio.ini`](../platformio.ini) first, then align the workflows.
- If the pin map changes, update [`include/PinDefinitions.h`](../include/PinDefinitions.h) first, then align [`include/TFT_Setup.h`](../include/TFT_Setup.h) and [`docs/hardware-setup.md`](./hardware-setup.md).
- If RTC, display, SD, live-upload, or OTA defaults change, update [`include/AppConfig.h`](../include/AppConfig.h) first, then align docs.
- If the onboard queue capacity or record format changes, align `StoreForwardQueue`, the ESP32 partition table, local status fields, and recovery documentation together.
- If live/status MQTT payload shape changes incompatibly, bump `Logic::kLivePayloadSchemaVersion`, keep version `1` compatibility documented, and align bridge/app tests before rollout.
- Keep README concise. Detailed hardware descriptions belong in [`docs/hardware-setup.md`](./hardware-setup.md).
- Add new durable docs only when they reduce ambiguity that cannot be enforced another way.
