# ESP32 production security and recovery

## Release status

The checked-in `esp32dev` environment is a development build. The
`esp32dev_production_candidate` environment compiles the firmware-side
`APEXI_PRODUCTION_SECURITY_REQUIRED=1` gate, but the bundled precompiled Arduino
SDK currently has Secure Boot and flash encryption disabled. Neither environment
is a production-approved image. ESP8266/NodeMCU is a compatibility and bench
target only and must not be commissioned as production hardware.

Run the machine-readable SDK audit before describing any ESP32 artifact as
production-eligible:

```sh
python3 scripts/check_production_security.py /path/to/sdkconfig \
  --build-flags '-D APEXI_PRODUCTION_SECURITY_REQUIRED=1' \
  --require-production
```

The command fails unless the runtime gate, Secure Boot v2, signed application
binaries, release-mode flash encryption, and bootloader rollback support are all
enabled. It also rejects insecure Secure Boot and UART/JTAG exceptions, including
`CONFIG_SECURE_BOOT_ALLOW_JTAG`, `CONFIG_SECURE_BOOT_ALLOW_ROM_BASIC`,
`CONFIG_SECURE_BOOT_INSECURE`, and the `CONFIG_SECURE_FLASH_UART_BOOTLOADER_ALLOW_*`
plaintext/cache options. Its success is necessary, not sufficient: the release evidence must
also contain per-device eFuse inspection and update/rollback results.

## Manufacturing profile

Production hardware requires an ESP-IDF-based or equivalently controlled build
whose SDK configuration includes:

- `CONFIG_SECURE_BOOT_V2_ENABLED=y`
- `CONFIG_SECURE_BOOT_BUILD_SIGNED_BINARIES=y`
- `CONFIG_SECURE_FLASH_ENC_ENABLED=y`
- `CONFIG_SECURE_FLASH_ENCRYPTION_MODE_RELEASE=y`
- `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y`
- firmware build flag `APEXI_PRODUCTION_SECURITY_REQUIRED=1`

Keep the Secure Boot signing key in the release signer/HSM; never place it in a
repository, CI log, provisioning bundle, or logger. Generate a unique flash
encryption key for each device. During manufacturing, install the signed
bootloader, partition table and application, enable Secure Boot v2 and
release-mode flash encryption, then disable insecure JTAG/UART download paths as
supported by the selected ESP32 revision. Read back and retain only non-secret
eFuse/security state and artifact digests as evidence. Treat eFuse operations as
irreversible and require a two-person reviewed manufacturing procedure before
burning production boards.

Provision owner credentials only after flash encryption is active and verified.
The firmware checks secure-boot and flash-encryption state before networking; a
production-candidate image on an unenrolled board reports
`NETWORK_DISABLED=production-security-required`. Generic development-mode flash
encryption is reported but cannot satisfy the runtime gate; the chip must report
`ESP_FLASH_ENC_MODE_RELEASE`. Secure Boot is the mechanism
that rejects unsigned or unapproved firmware at boot. HTTPS certificate
validation and application authentication do not replace it.

## Commissioning and local exposure

An unprovisioned ESP32 has no fallback access point, local web service, upload,
or OTA service. ESP32 local HTTP settings are disabled even after provisioning;
owner settings enter through the identity-bound USB workflow or the optional,
allow-listed server management path. `/api/live`, diagnostics, and serial status
may expose identity and security posture, but never Wi-Fi, OTA, MQTT, Cloudflare,
app bearer, recovery, flash-encryption, or signing secrets.

The existing USB provisioning payload is plaintext on the operator's local
serial connection. Until a recovery-credential proof protocol is implemented
and physically verified, commission only in a controlled manufacturing station,
block USB/UART access in the enclosure, revoke a bundle if its session may have
been observed, and do not claim the captured-local-traffic acceptance criterion.

## Update, rollback and recovery

Password-based Arduino OTA is disabled for ESP32. A production update path must
deliver signed images through the ESP-IDF OTA APIs, write only the inactive OTA
slot, verify its signature before selection, mark it pending verification, and
mark it valid only after sensor initialization, durable queue mount, and an
authenticated server heartbeat. A failed health window must allow the
bootloader to return to the last known-good slot. Anti-rollback security version
policy must be chosen and rehearsed before use; enabling it can make older images
permanently unbootable and therefore cannot be inferred from the current
rollback-enabled partition layout.

Owner recovery uses a deliberate five-second physical button hold to wipe owner
state. Re-provisioning or ownership transfer additionally requires a unique
per-device recovery credential and server/admin authorization before new scoped
credentials are issued. There is no fleet-wide recovery secret. The current
firmware implements the physical wipe but not recovery proof or server issuance,
so recovery authorization remains a production blocker.

For every production release, record the immutable device ID, hardware revision,
artifact SHA-256, signing-key identifier (not the key), security-version value,
eFuse security summary, old/new versions, update result, failed-boot rollback
result, recovery authorization result, and credential revocation result. Never
record secret values. Follow [Firmware releases and rollback](releases.md) for
the immutable release evidence, and [ESP32 identity and provisioning](provisioning.md)
for owner-state handling.

## Open production blockers

- migrate from the precompiled Arduino SDK to a reproducible secure ESP-IDF
  build and manufacturing signer;
- implement signed OTA selection, health confirmation, and anti-rollback policy;
- implement per-device recovery proof plus server/admin credential issuance;
- physically validate nearby-wireless denial, serial capture handling, eFuse
  state, signed/unsigned images, failed boot rollback, and credential redaction.

These blockers keep APE-82 In Progress.
