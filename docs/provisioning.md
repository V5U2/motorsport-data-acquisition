# ESP32 identity and provisioning

## Identity policy

Production ESP32 firmware derives its immutable logger ID from the 48-bit factory eFuse MAC and formats it as `mda-xxxxxxxxxxxx`. The ID is not an owner setting, is not accepted from the browser, and survives re-provisioning and factory reset. A factory-identical firmware image can therefore be installed on multiple boards without creating a shared `mda-logger` identity.

The ESP8266 build remains a development compatibility target. It derives a unique hostname from its chip ID, but the repeatable NVS provisioning workflow and production acceptance apply to the ESP32 target only.

Factory state contains only the hardware revision. Owner state contains the friendly name, Wi-Fi credential, OTA/settings credential, transport selection and credential, app bearer, and non-secret provisioning timestamp. The firmware refuses to load an owner record whose recorded ID differs from the current eFuse identity. The fleet inventory is non-secret and refuses a second reservation of an existing ID unless the operator explicitly selects the audited re-provision path.

## Create a secret bundle

Copy [`provisioning-bundle.example.json`](provisioning-bundle.example.json) to a secure temporary location outside the repository. Generate independent random values with an approved password manager or secrets manager. Do not reuse a human password, Cloudflare service token, app bearer, MQTT password, or OTA password between devices.

The app device bearer must be issued with subject `logger:<device ID>` and the `logger:ingest` scope. Put that subject in `upload.app_token_subject`; it is checked locally before the record is written, and the telemetry app independently rejects the real bearer unless its resolved subject matches the payload device ID. For MQTT, the broker username must exactly equal the logger ID and its ACL must restrict the credential to that logger's topics. The firmware continues to post HTTPS telemetry only to the telemetry app compatibility endpoint; provisioning does not add a gateway endpoint or move gateway responsibilities into firmware.

Keep the bundle only in an encrypted vault or ephemeral operator workspace. Back up the non-secret inventory normally, but do not copy secrets into it, issue trackers, source control, shell history, screenshots, or service logs.

## Commission over USB

Install `pyserial` in the operator environment, connect one ESP32 by USB, and flash the factory image. The firmware prints its immutable `DEVICE_ID` at 115200 baud and opens a 2.5-second provisioning window after every reset.

```sh
python3 scripts/provision_device.py /secure/path/logger.json \
  --port /dev/cu.usbserial-0001 \
  --inventory /secure/inventory/loggers.json
```

The tool resets the board, reads the identity, checks the complete bundle, atomically reserves the ID under an inventory lock, writes one `APEXI_PROVISION` record, waits for an accepted response, and then commits only ID, friendly name, hardware revision, and timestamp in the inventory. It never prints the bundle or its secrets. An explicit delivery failure rolls back the reservation. If acknowledgement is lost after transmission, the entry remains `indeterminate`; a `pending` entry left by an interrupted operator process is converted to the same state on the next attempt. Inspect the device before using `--reprovision` rather than assuming either outcome. The logger restarts after an accepted record.

After restart, check that serial output reports the expected `DEVICE_ID` and `PROVISIONING_STATUS=provisioned`. Check `/api/live` for the same non-secret ID, friendly name, hardware revision, and provisioning timestamp. Verify station Wi-Fi and the app HTTPS heartbeat. The API and settings page must never contain Wi-Fi, OTA, MQTT, Cloudflare, or app bearer values.

## Re-provision and ownership transfer

Before transfer, revoke the existing app bearer, Cloudflare service token or MQTT credential at their issuers. Create fresh credentials for the new owner and run the same command with `--reprovision`. The explicit flag permits the existing immutable ID in the inventory; it does not create or change device identity. Confirm the old credentials fail and the new app owner can claim the same physical logger.

If a bundle names a different `expected_device_id`, a bearer subject differs, an MQTT username differs, a required value is blank/malformed, or the factory hardware revision conflicts, firmware fails closed and does not start networking with that record. A partial persistence failure clears the owner namespace instead of booting with mixed credentials.

## Factory reset and recovery

With the ESP32 powered, hold the physical UI button continuously for five seconds during boot. The device clears the owner NVS namespace and the flash-backed runtime settings, then restarts. It preserves the eFuse-derived identity and factory hardware revision. Network, transport, app, Cloudflare, MQTT, OTA/settings, friendly-name, and remote-management state are removed. Re-provision over USB before the logger can join a network again.

A full flash erase also removes the factory hardware-revision record and onboard store-and-forward data, but it cannot change the eFuse identity. Treat full erase as destructive service recovery and capture or explicitly abandon queued telemetry first.

## Verification boundaries

Automated host tests cover canonical and distinct identity derivation, blank/malformed bundles, duplicate inventory entries, device-ID mismatch, bearer-subject mismatch, MQTT-username mismatch, and secret-free inventory output. PlatformIO builds verify both supported compile targets.

Production acceptance additionally requires two physical ESP32 boards, a provisioning/re-provision/factory-reset exercise, issuer-side credential revocation, and inspection of real serial, browser, and app traffic. Record board IDs and redacted results in the release evidence. Do not infer those outcomes from host tests.

The managed app environment must also register one distinct `logger:<device ID>` bearer subject and secret for every commissioned logger. A deployment that templates only one static logger bearer cannot demonstrate the two-device acceptance criterion even though both boards have unique firmware identities. Initial issuer registration is therefore an explicit app-operations dependency; automated token lifecycle remains separate server work. Do not mark APE-81 complete until two real subjects are registered, both devices check in as distinct loggers, and cross-use of either credential is rejected.

ESP32 fallback AP, password OTA, and local HTTP settings are disabled. A production-candidate build also refuses networking unless runtime Secure Boot and flash-encryption checks pass. The bundled Arduino SDK is still development-only, and USB bundle delivery is still plaintext on the local serial connection. Signed OTA, per-device recovery proof, actual encrypted NVS/eFuse enrollment, and their physical validation remain blockers in APE-82. Follow [ESP32 production security and recovery](production-security.md) and restrict this firmware to controlled development hardware.
