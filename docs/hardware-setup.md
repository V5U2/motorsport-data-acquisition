# Hardware Setup

## Core modules
- MCU: TinyS3 [D] ESP32-S3 development board as the preferred compact build target
- ADC: ADS1115 on I2C, using the Adafruit ADA1085 board
- Sensor interface: 2x DFRobot SEN0262 current-to-voltage modules
- Power: DFRobot DFR1015 buck converter for the regulated rail
- UI button: DFRobot DFR0029-W digital push button
- Optional RTC + SD: RTC Logging Shield for TinyS3 builds
- Optional display: 480x320 SPI TFT using ST7796S

## Recommended wiring

### Power front end
- Vehicle 12 V input -> fused lead or fuse holder -> off-the-shelf reverse-polarity/transient protection module -> buck converter module to 5 V
- 5 V rail -> ESP32 5 V/VBUS input, TFT supply, ADS1115 supply, and any optional RTC/storage hardware that needs 5 V
- 3.3 V rail from ESP32 -> logic-level pull-ups and any 3.3 V-only interface side
- Sensor supply should come from the protected 12 V rail unless the transmitter datasheet requires higher loop voltage
- If using the `DFR1015`, set and verify the 5 V output before connecting the ESP32 and peripherals
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
- Receiver module analog output -> ADS1115 A1 for oil temperature
- ADS1115 GND -> system ground

### Shared buses
- I2C bus:
  - ESP32 `SDA` -> ADS1115 SDA + optional RTC shield / alternate RTC SDA
  - ESP32 `SCL` -> ADS1115 SCL + optional RTC shield / alternate RTC SCL
- SPI bus:
  - ESP32 MOSI/MISO/SCLK -> optional TFT + optional microSD
  - Separate chip select lines for TFT and SD when those peripherals are fitted

## Default firmware pin map

| Function | Pin |
| --- | --- |
| I2C SDA | GPIO 8 |
| I2C SCL | GPIO 9 |
| SPI MOSI | GPIO 35 |
| SPI MISO | GPIO 37 |
| SPI SCLK | GPIO 36 |
| TFT CS | GPIO 3 |
| TFT DC | GPIO 4 |
| TFT RST | GPIO 5 |
| TFT BL | GPIO 6 |
| SD CS | GPIO 34 |
| UI Button | GPIO 7 |

Update the values in [`include/PinDefinitions.h`](../include/PinDefinitions.h) if the actual wiring differs.

## Core BOM

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | [TinyS3 [D] ESP32-S3 Development Board](https://core-electronics.com.au/catalog/product/view/sku/CE10240) | Main controller | Preferred compact board and the default firmware target in this repo |
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
| 1 | [RTC Logging Shield (Unexpected Maker CE09724)](https://core-electronics.com.au/shield-logger.html) | Combined RTC + microSD board for TinyS3 builds | Optional; firmware now targets its `RV-3028-C7` RTC and standard ESP32 SD path |
| 1 | [24 V boost regulator for loop-powered sensors (Pololu U3V9F24, item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) | Generates a dedicated 24 V sensor supply from the 12 V system rail | Optional; use only when a transmitter needs 24 V loop power and place it after the [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) |

Recommended example module:
- [DFRobot Gravity Analog Current to Voltage Converter (SEN0262)](https://core-electronics.com.au/gravity-analog-current-to-voltage-converter-for-4-20ma-application.html) for the current breakout-style implementation.

Optional hardware toggles:
- Set `AppConfig::kFeatures.displayEnabled` to `false` when no TFT is fitted.
- Set `AppConfig::kFeatures.rtcEnabled` to `false` when no RTC hardware is fitted.
- Set `AppConfig::kFeatures.sdLoggingEnabled` to `false` when no SD hardware is fitted.

For station Wi-Fi or live MQTT commissioning, copy [`include/AppSecrets.example.h`](../include/AppSecrets.example.h) to the git-ignored `include/AppSecrets.h`. Keep Wi-Fi and broker passwords in that local file. An authenticated production broker expects `APEXI_MQTT_USERNAME` to equal the normalized `AppConfig::kLiveUpload.deviceId`, with the matching password provisioned in the infrastructure vault.

When using receiver modules:
- Treat the field 4-20 mA transmitters as external inputs to the logger rather than part of the logger BOM.
- Treat the removable microSD card as runtime media rather than part of the logger BOM when SD logging hardware is installed.
- Confirm the module output range before wiring it to the ADS1115 or any direct ESP32 ADC input.
- The `DFR1015` power module does not replace the need for a fuse and upstream automotive protection when installed in a vehicle.
- If a sensor needs 24 V loop power from a 12 V vehicle supply, prefer the explicit [Pololu 5380 reverse-voltage protector](https://core-electronics.com.au/pololu-reverse-voltage-protector-4-60v-10a.html) plus [Pololu U3V9F24 (item 5588)](https://core-electronics.com.au/catalog/product/view/sku/POLOLU-5588) stack instead of a generic high-power adjustable module.
- The RTC Logging Shield is the preferred TinyS3 RTC + microSD option, and the firmware now defaults to its `RV-3028-C7` RTC path.
- Update the engineering conversion assumptions in [`include/AppConfig.h`](../include/AppConfig.h) if the module output scaling no longer matches the original shunt-based design.

## Commissioning checklist
1. Confirm the sensor supply voltage and compliance requirement from the actual transmitter datasheets.
2. Verify the receiver module output voltage at 4 mA and 20 mA before connecting it to the ADS1115.
3. Confirm the TFT controller is ST7796S. If it is ILI9488 or another controller, update [`include/TFT_Setup.h`](../include/TFT_Setup.h).
4. Set the RTC to the correct time before field logging.
5. Inject 4, 8, 12, 16, and 20 mA into each channel and verify the receiver modules and displayed engineering units match the configured ranges.
