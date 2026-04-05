# Hardware Setup

## Core modules
- MCU: ESP32-S3 DevKitC-1 class board
- ADC: ADS1115 on I2C
- Sensor interface: 2x off-the-shelf 4-20 mA receiver/current-to-voltage modules
- Optional RTC: DS3231 on I2C with backup cell
- Optional display: 480x320 SPI TFT using ST7796S
- Optional storage: microSD on the shared SPI bus

## Recommended wiring

### Power front end
- Vehicle 12 V input -> fused lead or fuse holder -> off-the-shelf reverse-polarity/transient protection module -> buck converter module to 5 V
- 5 V rail -> ESP32 5 V/VBUS input, TFT supply, ADS1115 supply, DS3231 supply
- 3.3 V rail from ESP32 -> logic-level pull-ups and any 3.3 V-only interface side
- Sensor supply should come from the protected 12 V rail unless the transmitter datasheet requires higher loop voltage

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
  - ESP32 `SDA` -> ADS1115 SDA + optional DS3231 SDA
  - ESP32 `SCL` -> ADS1115 SCL + optional DS3231 SCL
- SPI bus:
  - ESP32 MOSI/MISO/SCLK -> optional TFT + optional microSD
  - Separate chip select lines for TFT and SD when those peripherals are fitted

## Default firmware pin map

| Function | Pin |
| --- | --- |
| I2C SDA | GPIO 8 |
| I2C SCL | GPIO 9 |
| SPI MOSI | GPIO 11 |
| SPI MISO | GPIO 13 |
| SPI SCLK | GPIO 12 |
| TFT CS | GPIO 10 |
| TFT DC | GPIO 14 |
| TFT RST | GPIO 21 |
| TFT BL | GPIO 47 |
| SD CS | GPIO 15 |
| UI Button | GPIO 16 |

Update the values in [`include/PinDefinitions.h`](../include/PinDefinitions.h) if the actual wiring differs.

## Core BOM

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | [ESP32-S3 DevKitC-1 class board](https://www.digikey.com/en/products/detail/espressif-systems/ESP32-S3-DEVKITC-1-N8/15199021) | Main controller | Matches the default PlatformIO environment |
| 2 | [4-20 mA receiver/current-to-voltage modules](https://www.digikey.com/en/products/detail/dfrobot/SEN0262/9559248) | Converts each loop signal into a board-friendly voltage | One module per sensor channel |
| 1 | [ADS1115 breakout](https://www.digikey.com/en/products/detail/adafruit-industries-llc/1085/5761229) | Reads the module voltage outputs | Keep this when the receiver modules expose analog outputs |
| 1 | [Momentary push button](https://www.digikey.com/en/products/detail/adafruit-industries-llc/471/7349483) | UI mode toggle and latched fault clear | Connect to the configured button input |
| 1 | [Fused 12 V input path](https://www.bluesea.com/products/5064/) | Protects the incoming 12 V feed | Inline fuse holder or a prebuilt fused automotive input lead |
| 1 | [12 V reverse-polarity/transient protection module](https://www.pololu.com/product/5380) | Protects the electronics from common vehicle power faults | Prefer a prebuilt automotive power protection module |
| 1 | [12 V to 5 V buck converter module](https://www.digikey.com/en/products/detail/pololu/2851/10451177) | Generates the regulated supply rail | Should comfortably cover TFT backlight and SD peaks |
| 1 | [Enclosure and wiring hardware](https://www.printables.com/tag/projectbox) | Physical integration | Printed or purchased enclosure, plus harness, terminals, mounting hardware, and grounding hardware |

## Optional additions

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | [3.5 inch 480x320 SPI TFT with ST7796S controller](https://www.waveshare.com/3.5inch-capacitive-touch-lcd.htm) | Local dashboard display | Optional; if fitted, leave `displayEnabled` on in [`include/AppConfig.h`](../include/AppConfig.h) |
| 1 | [DS3231 RTC module with backup cell](https://www.digikey.com/en/products/detail/adafruit-industries-llc/5188/15189155) | Maintains timestamps during power loss | Optional; if omitted, disable `rtcEnabled` and the firmware uses fallback timestamps |
| 1 | [microSD breakout or integrated SD socket](https://www.digikey.com/en/products/detail/adafruit-industries-llc/254/5761230) | Connects removable storage on the shared SPI bus | Optional; if omitted, disable `sdLoggingEnabled` and the web UI will not expose log files |

Recommended example module:
- [DFRobot Gravity Analog Current to Voltage Converter](https://www.digikey.com/en/products/detail/dfrobot/SEN0262/9559248) for a simple breakout-style implementation.

Optional hardware toggles:
- Set `AppConfig::kFeatures.displayEnabled` to `false` when no TFT is fitted.
- Set `AppConfig::kFeatures.rtcEnabled` to `false` when no DS3231 is fitted.
- Set `AppConfig::kFeatures.sdLoggingEnabled` to `false` when no SD hardware is fitted.

When using receiver modules:
- Treat the field 4-20 mA transmitters as external inputs to the logger rather than part of the logger BOM.
- Treat the removable microSD card as runtime media rather than part of the logger BOM when SD logging hardware is installed.
- Confirm the module output range before wiring it to the ADS1115 or any direct ESP32 ADC input.
- Update the engineering conversion assumptions in [`include/AppConfig.h`](../include/AppConfig.h) if the module output scaling no longer matches the original shunt-based design.

## Commissioning checklist
1. Confirm the sensor supply voltage and compliance requirement from the actual transmitter datasheets.
2. Verify the receiver module output voltage at 4 mA and 20 mA before connecting it to the ADS1115.
3. Confirm the TFT controller is ST7796S. If it is ILI9488 or another controller, update [`include/TFT_Setup.h`](../include/TFT_Setup.h).
4. Set the DS3231 to the correct time before field logging.
5. Inject 4, 8, 12, 16, and 20 mA into each channel and verify the receiver modules and displayed engineering units match the configured ranges.
