# Hardware Setup

## Core modules
- MCU: NodeMCU 1.0 / ESP-12E DevKit V2 (`nodemcuv2`) or classic ESP32 DevKit / ESP32-WROOM-32 (`esp32dev`)
- ADC: ADS1115 on I2C, using the Adafruit ADA1085 board
- Sensor interface: 2x DFRobot SEN0262 current-to-voltage modules
- Power: DFRobot DFR1015 buck converter for the regulated rail
- UI button: DFRobot DFR0029-W digital push button
- RTC: RV-3028-C7 on the Unexpected Maker RTC Logger Shield, sharing the primary I2C bus with the ADS1115
- Storage: microSD slot on the Unexpected Maker RTC Logger Shield, using FAT32 media
- Optional display: 480x320 SPI TFT using ST7796S

## Recommended wiring

### Power front end
- Vehicle 12 V input -> fused lead or fuse holder -> off-the-shelf reverse-polarity/transient protection module -> buck converter module to 5 V
- 5 V rail -> NodeMCU `VIN` input and any peripheral explicitly rated for 5 V power; do not back-feed a board-dependent `VU`/USB rail
- NodeMCU `3V3` -> ADS1115 VDD and all ESP8266-side I2C/SPI logic; do not pull an ESP8266 GPIO up to 5 V
- Power the ADS1115 at 3.3 V. The SEN0262's 0-3 V output remains inside the ADC supply range while its I2C pull-ups remain safe for the ESP8266
- The selected field sensors should be ordered for direct operation from the protected 12 V rail; confirm the vendor's supported voltage range and 4-20 mA loop compliance rather than relying on a nominal "12 V" label
- If using the `DFR1015`, set and verify the 5 V output before connecting the NodeMCU and peripherals
- For a 24 V loop-powered sensor, the preferred branch is fused 12 V -> [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) -> [Pololu U3V9F24 step-up regulator (item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) -> sensor supply

### Current loop receivers
- Use one off-the-shelf 4-20 mA receiver/current-to-voltage module per sensor channel
- Wire each transmitter loop into its receiver module according to that module's datasheet
- Feed each module's analog voltage output into the ADS1115 input channel configured for that sensor
- Keep the module output range within the ADS1115 input range selected by the firmware
- Prefer receiver modules that already include the current sense, filtering, and input protection stages

### Sensor loop topology
- Protected 12 V -> sensor `+`
- Sensor loop output/current return -> 4-20 mA receiver module input
- Receiver module analog output -> ADS1115 A0 for oil pressure
- Second receiver module analog output -> ADS1115 A1 for oil temperature
- ADS1115 GND -> system ground

### Shared buses
- I2C bus:
  - NodeMCU `D2` / GPIO 4 -> ADS1115 SDA + optional RTC SDA
  - NodeMCU `D1` / GPIO 5 -> ADS1115 SCL + optional RTC SCL
- SPI bus:
  - NodeMCU `D7` / GPIO 13 -> optional TFT + microSD MOSI
  - NodeMCU `D6` / GPIO 12 -> optional TFT + microSD MISO
  - NodeMCU `D5` / GPIO 14 -> optional TFT + microSD SCLK
  - Separate chip select lines for TFT and SD when those peripherals are fitted

## Field sensor ordering specification

The field transmitters are external inputs to the logger rather than part of the core electronics BOM. The selected configuration keeps the industrial 4-20 mA interface while ordering both transmitters for direct operation from the protected 12 V vehicle supply.

| Qty | Sensor | Required configuration |
| --- | --- | --- |
| 1 | Oil pressure transmitter | 0-0.8 MPa (0-8 bar), 1/8-inch NPT male, 4-20 mA output, protected 12 V supply compatible, 5 m cable |
| 1 | Oil temperature transmitter | 0-150 degrees Celsius, 1/8-inch NPT male, 4-20 mA output, protected 12 V supply compatible, 5 m cable, dimensional limit below |

Temperature probe dimensional limit:

- Maximum total insertion length is **23.5 mm**, measured from the probe tip to the mounting shoulder and including the threaded section.
- Allocate **15 mm** to the threaded section, leaving no more than **8.5 mm** of unthreaded probe beyond the threads.
- The rejected existing 24 V temperature transmitter measures 45 mm total: 35 mm unthreaded probe plus 10 mm of thread. Do not reorder that geometry for the replacement 12 V, 4-20 mA transmitter.
- Require a vendor dimensioned drawing or written confirmation that both the total 23.5 mm limit and the 8.5 mm exposed-probe limit are met. A bare "probe length" value is ambiguous and is not sufficient for approval.
- Confirm the transmitter can drive the selected 4-20 mA receiver at 20 mA across the vendor's full stated 12 V operating range before ordering. If it cannot, use the documented 24 V boost branch instead.

## Default NodeMCU pin map

Use the board's printed `D` label when wiring. The firmware stores the corresponding raw GPIO number.

| Function | NodeMCU label | GPIO | Default use |
| --- | --- | --- | --- |
| I2C SDA | D2 | GPIO 4 | ADS1115 SDA; required |
| I2C SCL | D1 | GPIO 5 | ADS1115 SCL; required |
| SPI MOSI | D7 | GPIO 13 | Optional TFT/microSD |
| SPI MISO | D6 | GPIO 12 | Optional TFT/microSD |
| SPI SCLK | D5 | GPIO 14 | Optional TFT/microSD |
| TFT CS | D8 | GPIO 15 | Optional; must remain low during boot |
| TFT DC | D3 | GPIO 0 | Optional; must remain high during boot |
| TFT RST | D4 | GPIO 2 | Optional; must remain high during boot |
| Built-in status LED | D4 | GPIO 2 | Active low; steady on after firmware setup confirms MCU power/running state |
| TFT BL | Supply | n/a | Hard-wire to the display's rated supply; no GPIO default |
| SD CS | D0 | GPIO 16 | Optional microSD chip select |
| UI button | RX | GPIO 3 | Optional active-low button; serial diagnostics use TX only |

For the current two-sensor build, both receiver signals terminate at the ADS1115, so only power, ground, D1, and D2 are required between the NodeMCU and ADS1115. D3, D4, and D8 are ESP8266 boot-strapping pins; never attach a peripheral that drives them to the wrong level during reset. The built-in LED and optional TFT reset currently share D4, so remap `PIN_TFT_RST` before enabling the TFT. Update [`include/PinDefinitions.h`](../include/PinDefinitions.h) and re-verify the boot state if the optional pin assignment changes.

Update the values in [`include/PinDefinitions.h`](../include/PinDefinitions.h) if the actual wiring differs.

## Classic ESP32 DevKit pin map

Use the raw GPIO numbers printed on a classic ESP32 DevKit/WROOM-class board.

| Function | ESP32 GPIO | Default use |
| --- | --- | --- |
| I2C SDA | GPIO 21 | ADS1115 SDA + RTC SDA |
| I2C SCL | GPIO 22 | ADS1115 SCL + RTC SCL |
| SPI MOSI | GPIO 23 | Optional TFT + microSD MOSI |
| SPI MISO | GPIO 19 | Optional TFT + microSD MISO |
| SPI SCLK | GPIO 18 | Optional TFT + microSD SCLK |
| microSD CS | GPIO 5 | RTC Logger Shield microSD CS |
| TFT CS | GPIO 27 | Optional TFT chip select |
| TFT DC | GPIO 26 | Optional TFT data/command |
| TFT RST | GPIO 25 | Optional TFT reset |
| UI button | GPIO 32 | Optional active-low button |
| Built-in status LED | GPIO 2 | Common DevKit LED assignment; active high |

The sensor receiver boards still connect to ADS1115 A0 and A1 rather than to an ESP32 ADC pin. Power the ADS1115 from the ESP32 `3V3` pin so its I2C pull-ups remain at 3.3 V. Some DevKit variants omit the GPIO 2 LED; that does not affect logging.

The detected ESP32 target has 16 MB flash. Its firmware reserves dual 2 MB OTA slots and an approximately 12 MB LittleFS partition, of which at most 10 MB is used for the circular HTTPS store-and-forward queue. This makes microSD optional for transient outage recovery, and SD logging is disabled by default for the ESP32 target. Fit microSD and enable the feature only when long-duration CSV archives or removable media are required; the onboard queue is not exposed as a user filesystem and automatically acknowledges replayed records. Firmware never formats this partition automatically: a mount failure reports a store-and-forward fault and preserves the partition for an explicit service recovery or intentional erase. Follow the [store-and-forward recovery contract](store-forward-recovery.md) for retention estimates, corruption evidence, power-loss outcomes, and the deliberately destructive erase procedure.

## Core BOM

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | NodeMCU 1.0 / ESP-12E DevKit V2 | Main controller | Supported default target; PlatformIO board ID `nodemcuv2` |
| 2 | [Gravity Analog Current to Voltage Converter (DFRobot SEN0262)](https://core-electronics.com.au/gravity-analog-current-to-voltage-converter-for-4-20ma-application.html) | Converts each loop signal into a board-friendly voltage | One module per sensor channel |
| 1 | [ADS1115 16-bit ADC breakout (Adafruit ADA1085)](https://core-electronics.com.au/ads1115-16-bit-adc-4-channel-with-programmable-gain-amplifier.html) | Reads the module voltage outputs | ADS1115 board for the receiver outputs |
| 1 | [Digital Push Button, white (DFRobot DFR0029-W)](https://core-electronics.com.au/digital-push-button-white.html) | UI mode toggle and latched fault clear | Connect to the configured button input |
| 1 | [Fused 12 V input path](https://www.bluesea.com/products/5064/) | Protects the incoming 12 V feed | Inline fuse holder or a prebuilt fused automotive input lead |
| 1 | [12 V reverse-polarity/transient protection module (Pololu 5380)](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) | Protects the electronics from common vehicle power faults | Prefer a prebuilt automotive power protection module |
| 1 | [DC-DC Multi-output Buck Converter (DFRobot DFR1015)](https://core-electronics.com.au/dc-dc-multi-output-buck-converter-33v5v9v12v.html) | Generates the regulated supply rail | Use the 5 V rail and keep upstream automotive protection |
| 1 | [Enclosure and wiring hardware](https://www.printables.com/tag/projectbox) | Physical integration | Printed enclosure or purchased box, plus harness, terminals, mounting hardware, and grounding hardware |

## Optional additions

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | [3.5 inch 480x320 SPI TFT with ST7796S controller](https://core-electronics.com.au/catalog/product/view/sku/WS-15811) | Local dashboard display | Optional; if fitted, leave `displayEnabled` on in [`include/AppConfig.h`](../include/AppConfig.h) |
| 1 | Unexpected Maker RTC Logger Shield with RV-3028-C7 | Timestamps without network time | Enabled; shield GPIO 8/SDA connects to D2 and GPIO 9/SCL connects to D1 |
| 1 | FAT32 microSD card in the Unexpected Maker RTC Logger Shield | Durable local CSV storage | Enabled; shield pins 36/37/35/34 map to D5/D6/D7/D0 respectively |
| 1 | [24 V boost regulator for loop-powered sensors (Pololu U3V9F24, item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) | Generates a dedicated 24 V sensor supply from the 12 V system rail | Optional; use only when a transmitter needs 24 V loop power and place it after the [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) |

Recommended example module:
- [DFRobot Gravity Analog Current to Voltage Converter (SEN0262)](https://core-electronics.com.au/gravity-analog-current-to-voltage-converter-for-4-20ma-application.html) for the current breakout-style implementation.

Optional hardware toggles:
- Set `AppConfig::kFeatures.displayEnabled` to `false` when no TFT is fitted.
- Set `AppConfig::kFeatures.rtcEnabled` to `false` when no RTC hardware is fitted.
- Set `AppConfig::kFeatures.sdLoggingEnabled` to `false` when no SD hardware is fitted.

When microSD logging is disabled, the local dashboard leaves the CSV card visible but disabled and does not request the SD file list. The main page otherwise stays focused on compact sensor readings and a fault summary; use its **Diagnostics** action for detailed connectivity, hardware, time, storage, transport, and per-sensor state.

ESP32 station and owner credentials come from the identity-bound USB [provisioning runbook](provisioning.md). An unprovisioned ESP32 fails closed; it does not start networking, a fallback AP, local web/settings, upload, or OTA. ESP32 password OTA and local settings remain disabled after provisioning. A production-candidate build also refuses networking until runtime Secure Boot and flash-encryption checks pass. The ESP8266 build retains those legacy features strictly as a development target and is not supported for production. See [ESP32 production security and recovery](production-security.md). At every networked boot, station-mode firmware obtains NTP time and writes configured local time into the RV-3028, even when its retained calendar is already valid. It refreshes the RTC hourly while online and uses the hardware clock as holdover while offline. When an RTC is absent or cannot be written, valid NTP/system time still supplies timestamps while the UI correctly reports `rtc_ready=false`. UI and CSV timestamps use configured local wall time; live transport payloads use timezone-qualified UTC RFC 3339 timestamps.

For ESP32 commissioning, use [`scripts/provision_device.py`](../scripts/provision_device.py) and never commit the secret bundle. The eFuse-derived `mda-xxxxxxxxxxxx` identity becomes the Wi-Fi hostname, upload identity, MQTT username requirement, and app bearer subject. The Access application must use a **Service Auth** policy for its device-specific token; the device sends both Access headers and its app bearer on every request. Local credential fields remain write-only: neither their values nor Wi-Fi, OTA, MQTT, or app credentials are returned to the browser or local status API.

Remote management is optional and starts disabled. For production ESP32, set
`remote_management_enabled` in the identity-bound USB provisioning bundle only
when the owner has opted in; the disabled local settings route cannot enable it.
The ESP8266 development target retains its digest-authenticated local toggle.
After an enabled logger checks in, enter its temporary eight-character code in
the app's single **User profile → Paired devices** field; no logger ID is
required. The code refreshes every ten minutes. After claim, explicitly enable
remote configuration for that logger in the app. Each management heartbeat
reports the logger's current live-upload and NTP/timezone values so the app can
pre-fill its form without exposing Cloudflare Access, app, MQTT, Wi-Fi, OTA, or
bearer credentials. The device checks desired configuration identity,
completeness, and monotonic version before persisting it. No app command may
change the broker host, MQTT credential, OTA password, sensor calibration, or
remote-management opt-in.

On ESP32 HTTPS deployments, app bearer rotation uses the same outbound status
path and does not require another pin or inbound service. Do not remove power
during a planned cutover merely to force an update: the staged rotation survives
power loss and resumes automatically, while the old bearer is retained until the
candidate is proven. If both credentials are rejected after the server overlap,
perform the documented physical owner reset and authorized USB re-provisioning;
do not erase eFuses or substitute a fleet-wide credential.

An armed live-session plan is delivered in that desired document and acknowledged under `management.assignment` in the next status heartbeat. Confirm its target, role, expiry, and state before a run. Once the app observes the firmware's new per-boot source, the assignment becomes claimed and reports both the unchanged source ID and canonical recording ID. If assignment delivery is offline, expired, revoked, or invalid, continue the local run normally; never restart merely to obtain server assignment state, and recover the unassigned source in the app afterward.

For the managed development environment, use broker hostname `apexlabs-dev`, port `1883`, and topic prefix `motorsport/logger` from an allowed LAN or VPN. Set `AppConfig::kWifi.mode` to `WifiMode::Station` and `AppConfig::kFeatures.liveUploadEnabled` to `true` before building the track firmware.

Commissioning is complete only when the device UI reports `MQTT LIVE` or `HTTPS LIVE`, `/api/live` reports a current `system.upload_session_id` and increasing `system.upload_sequence`, and the same source session appears in the telemetry app. Each reboot intentionally creates a new source session. HTTPS is outbound-only and polls desired configuration in status responses, so it does not require an SSH tunnel or inbound device route.

When using receiver modules:
- Treat the field 4-20 mA transmitters as external inputs to the logger rather than part of the logger BOM.
- Treat the removable microSD card as runtime media rather than part of the logger BOM when SD logging hardware is installed.
- Confirm the module output range before wiring it to the ADS1115. The supported design does not use the NodeMCU's direct `A0` input.
- The `DFR1015` power module does not replace the need for a fuse and upstream automotive protection when installed in a vehicle.
- If a sensor needs 24 V loop power from a 12 V vehicle supply, prefer the explicit [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) plus [Pololu U3V9F24 (item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) stack instead of a generic high-power adjustable module.
- The NodeMCU has fewer GPIOs than the former TinyS3 design. Validate boot-strap levels and power requirements before enabling the optional TFT, RTC, SD, or button paths together.
- Update the engineering conversion assumptions in [`include/AppConfig.h`](../include/AppConfig.h) if the module output scaling no longer matches the original shunt-based design.

## Commissioning checklist
1. Follow the [provisioning runbook](provisioning.md), record the non-secret immutable device ID, and verify it matches the app credential subject.
2. Confirm the sensor supply voltage and compliance requirement from the actual transmitter datasheets.
3. Verify the receiver module output voltage at 4 mA and 20 mA before connecting it to the ADS1115.
4. Confirm the TFT controller is ST7796S. If it is ILI9488 or another controller, update [`include/TFT_Setup.h`](../include/TFT_Setup.h).
5. Set the RTC to the correct time before field logging.
6. Inject 4, 8, 12, 16, and 20 mA into each channel and verify the receiver modules and displayed engineering units match the configured ranges.
7. Confirm the serial boot report shows `wifiReady=1`, station mode, and a DHCP address before installing the logger in the vehicle.
