# Hardware Setup

## Core modules
- MCU: ESP32-S3 DevKitC-1 class board
- ADC: ADS1115 on I2C
- RTC: DS3231 on I2C with backup cell
- Display: 480x320 SPI TFT using ST7796S
- Storage: microSD on the shared SPI bus
- Sensor interface: 2x off-the-shelf 4-20 mA receiver/current-to-voltage modules

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
  - ESP32 `SDA` -> ADS1115 SDA + DS3231 SDA
  - ESP32 `SCL` -> ADS1115 SCL + DS3231 SCL
- SPI bus:
  - ESP32 MOSI/MISO/SCLK -> TFT + microSD
  - Separate chip select lines for TFT and SD

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

## Off-the-shelf BOM

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | [ESP32-S3 DevKitC-1 class board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html) | Main controller | Matches the default PlatformIO environment |
| 1 | [3.5 inch 480x320 SPI TFT with ST7796S controller](https://www.waveshare.com/3.5inch-capacitive-touch-lcd.htm) | Local dashboard display | Prefer a version with integrated microSD if available; if the controller differs, update [`include/TFT_Setup.h`](../include/TFT_Setup.h) |
| 2 | [4-20 mA receiver/current-to-voltage modules](https://www.dfrobot.com/product-1755.html) | Converts each loop signal into a board-friendly voltage | One module per sensor channel |
| 1 | [ADS1115 breakout](https://www.adafruit.com/product/1085) | Reads the module voltage outputs | Keep this when the receiver modules expose analog outputs |
| 1 | [DS3231 RTC module with backup cell](https://www.adafruit.com/product/5188) | Maintains timestamps during power loss | I2C device at address `0x68` by default |
| 1 | [microSD breakout or integrated SD socket](https://www.adafruit.com/product/254) | Connects removable storage on the shared SPI bus | Optional if the chosen TFT already has SD hardware |
| 1 | [Momentary push button](https://www.adafruit.com/product/471) | UI mode toggle and latched fault clear | Connect to the configured button input |
| 1 | [Fused 12 V input path](https://www.bluesea.com/products/5064/) | Protects the incoming 12 V feed | Inline fuse holder or a prebuilt fused automotive input lead |
| 1 | [12 V reverse-polarity/transient protection module](https://www.pololu.com/product/5380) | Protects the electronics from common vehicle power faults | Prefer a prebuilt automotive power protection module |
| 1 | [12 V to 5 V buck converter module](https://www.pololu.com/product/2851) | Generates the regulated supply rail | Should comfortably cover TFT backlight and SD peaks |
| 1 | [Enclosure and wiring hardware](https://www.polycase.com/general-use-enclosures) | Physical integration | Includes harness, terminals, mounting hardware, and grounding hardware |

Recommended example module:
- [DFRobot Gravity Analog Current to Voltage Converter](https://www.dfrobot.com/product-1755.html) for a simple breakout-style implementation.

When using receiver modules:
- Treat the field 4-20 mA transmitters as external inputs to the logger rather than part of the logger BOM.
- Treat the removable microSD card as runtime media rather than part of the logger BOM.
- Confirm the module output range before wiring it to the ADS1115 or any direct ESP32 ADC input.
- Update the engineering conversion assumptions in [`include/AppConfig.h`](../include/AppConfig.h) if the module output scaling no longer matches the original shunt-based design.

## Commissioning checklist
1. Confirm the sensor supply voltage and compliance requirement from the actual transmitter datasheets.
2. Verify the receiver module output voltage at 4 mA and 20 mA before connecting it to the ADS1115.
3. Confirm the TFT controller is ST7796S. If it is ILI9488 or another controller, update [`include/TFT_Setup.h`](../include/TFT_Setup.h).
4. Set the DS3231 to the correct time before field logging.
5. Inject 4, 8, 12, 16, and 20 mA into each channel and verify the receiver modules and displayed engineering units match the configured ranges.
