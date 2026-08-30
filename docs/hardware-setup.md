# Hardware Setup

## Core modules
- MCU: NodeMCU 1.0 / ESP-12E DevKit V2 (`nodemcuv2`) as the supported firmware target
- ADC: ADS1115 on I2C, using the Adafruit ADA1085 board
- Sensor interface: 2x DFRobot SEN0262 current-to-voltage modules
- Power: DFRobot DFR1015 buck converter for the regulated rail
- UI button: DFRobot DFR0029-W digital push button
- RTC: RV-3028-C7 on the Unexpected Maker RTC Logger Shield, sharing the primary I2C bus with the ADS1115
- Optional storage: 3.3 V-compatible SPI microSD module
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
| 1 | 3.3 V-compatible SPI microSD module | Durable local CSV storage | Optional; uses D5/D6/D7 plus D0 chip select |
| 1 | [24 V boost regulator for loop-powered sensors (Pololu U3V9F24, item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) | Generates a dedicated 24 V sensor supply from the 12 V system rail | Optional; use only when a transmitter needs 24 V loop power and place it after the [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) |

Recommended example module:
- [DFRobot Gravity Analog Current to Voltage Converter (SEN0262)](https://core-electronics.com.au/gravity-analog-current-to-voltage-converter-for-4-20ma-application.html) for the current breakout-style implementation.

Optional hardware toggles:
- Set `AppConfig::kFeatures.displayEnabled` to `false` when no TFT is fitted.
- Set `AppConfig::kFeatures.rtcEnabled` to `false` when no RTC hardware is fitted.
- Set `AppConfig::kFeatures.sdLoggingEnabled` to `false` when no SD hardware is fitted.

The default is station mode using credentials from the ignored `include/AppSecrets.h`. The ESP8266 performs a normal all-channel scan rather than pinning a BSSID or channel. At every networked boot, station-mode firmware obtains NTP time and writes Perth local time (AWST, UTC+8 with no daylight-saving adjustment) into the RV-3028, even when its retained calendar is already valid. It refreshes the RTC hourly while online and uses the hardware clock as holdover while offline. The web UI reports NTP synchronization state and the last successful RTC update. If association fails within 30 seconds, it exposes the open fallback SoftAP `MDA-LOGGER` on 2.4 GHz channel 6 at `http://192.168.44.1`. Leave `apPassword` empty for an open recovery AP or set an 8+ character WPA2 password. Change `AppConfig::kWifi.apAddress` if that subnet is already in use.

For station Wi-Fi or live MQTT commissioning, copy [`include/AppSecrets.example.h`](../include/AppSecrets.example.h) to the git-ignored `include/AppSecrets.h`. Keep Wi-Fi and broker passwords in that local file. An authenticated production broker requires `APEXI_MQTT_USERNAME` to equal the normalized `AppConfig::kLiveUpload.deviceId`; firmware startup rejects a mismatched identity before connecting. Provision the matching password in the infrastructure vault.

For the managed development environment, use broker hostname `apexlabs-dev`, port `1883`, and topic prefix `motorsport/logger` from an allowed LAN or VPN. Set `AppConfig::kWifi.mode` to `WifiMode::Station` and `AppConfig::kFeatures.liveUploadEnabled` to `true` before building the track firmware.

Commissioning is complete only when the device UI reports `MQTT LIVE`, `/api/live` reports a current `system.upload_session_id` and increasing `system.upload_sequence`, and the same source session appears in the telemetry app. Each reboot intentionally creates a new source session. The cross-system start, monitoring, attachment, and finalization procedure is maintained in the telemetry app [Live Event Operations runbook](https://github.com/V5U2/motorsport-telemetry-app/blob/main/docs/live-events.md).

When using receiver modules:
- Treat the field 4-20 mA transmitters as external inputs to the logger rather than part of the logger BOM.
- Treat the removable microSD card as runtime media rather than part of the logger BOM when SD logging hardware is installed.
- Confirm the module output range before wiring it to the ADS1115. The supported design does not use the NodeMCU's direct `A0` input.
- The `DFR1015` power module does not replace the need for a fuse and upstream automotive protection when installed in a vehicle.
- If a sensor needs 24 V loop power from a 12 V vehicle supply, prefer the explicit [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) plus [Pololu U3V9F24 (item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) stack instead of a generic high-power adjustable module.
- The NodeMCU has fewer GPIOs than the former TinyS3 design. Validate boot-strap levels and power requirements before enabling the optional TFT, RTC, SD, or button paths together.
- Update the engineering conversion assumptions in [`include/AppConfig.h`](../include/AppConfig.h) if the module output scaling no longer matches the original shunt-based design.

## Commissioning checklist
1. Confirm the sensor supply voltage and compliance requirement from the actual transmitter datasheets.
2. Verify the receiver module output voltage at 4 mA and 20 mA before connecting it to the ADS1115.
3. Confirm the TFT controller is ST7796S. If it is ILI9488 or another controller, update [`include/TFT_Setup.h`](../include/TFT_Setup.h).
4. Set the RTC to the correct time before field logging.
5. Inject 4, 8, 12, 16, and 20 mA into each channel and verify the receiver modules and displayed engineering units match the configured ranges.
6. Confirm the serial boot report shows `wifiReady=1`, station mode, and a DHCP address before installing the logger in the vehicle.
