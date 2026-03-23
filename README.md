# Motorsport Data Acquisition

ESP32-S3 Arduino/PlatformIO firmware for a two-channel 4-20 mA motorsport logger and dashboard.

## Features
- Reads oil pressure and oil temperature from 4-20 mA sensors through an ADS1115 and 165 ohm shunts
- Displays live gauges and diagnostics on a 480x320 SPI TFT
- Logs CSV data to microSD with DS3231 RTC timestamps
- Serves a lightweight Wi-Fi dashboard and CSV download endpoints
- Keeps pin mapping, sensor calibration, and refresh rates in one config file

## Required hardware

### Core bill of materials

| Qty | Item | Notes |
| --- | --- | --- |
| 1 | ESP32-S3 DevKitC-1 class board | Main controller supported by [`platformio.ini`](platformio.ini) and the default pin map |
| 1 | 3.5 inch 480x320 SPI TFT with ST7796S controller | Main dashboard display; update [`include/TFT_Setup.h`](include/TFT_Setup.h) if the panel/controller differs |
| 1 | ADS1115 breakout | Two-channel analog front end for the 4-20 mA shunt measurements |
| 1 | DS3231 RTC module with backup coin cell | Provides persistent timestamps for CSV logging |
| 1 | microSD card | Log storage |
| 1 | microSD breakout or integrated SD socket | Needed only if the chosen TFT does not already include SD hardware |
| 2 | 4-20 mA transmitters | One oil pressure sensor and one oil temperature sensor |
| 2 | 165 ohm 0.1% shunt resistors, minimum 0.125 W | Converts 4-20 mA loops to ADC voltage input |
| 2 | 100 ohm series resistors | Recommended input filtering/protection at ADS1115 A0 and A1 |
| 2 | 0.1 uF capacitors | Input RC filter capacitors for the ADS1115 channels |
| 1 | Momentary push button | UI screen switch and fault clear input |
| 1 | Inline fuse and holder | Protects the vehicle-side 12 V feed |
| 1 | Reverse-polarity protection stage | Automotive survivability on the power input |
| 1 | Automotive TVS diode | Load-dump and transient suppression on the 12 V input |
| 1 | 12 V to 5 V buck converter | Supplies the ESP32, TFT, ADC, and RTC from vehicle power |
| 1 | Enclosure and wiring set | Connectors, terminals, mounting hardware, and harness materials |

### Simplified bill of materials

| Qty | Item | Notes |
| --- | --- | --- |
| 1 | ESP32-S3 DevKitC-1 class board | Main controller supported by [`platformio.ini`](platformio.ini) and the default pin map |
| 1 | 3.5 inch 480x320 SPI TFT with ST7796S controller | Main dashboard display; update [`include/TFT_Setup.h`](include/TFT_Setup.h) if the panel/controller differs |
| 1 | ADS1115 breakout | Keep this if the 4-20 mA receiver modules expose analog voltage outputs |
| 1 | DS3231 RTC module with backup coin cell | Provides persistent timestamps for CSV logging |
| 1 | microSD card | Log storage |
| 1 | microSD breakout or integrated SD socket | Needed only if the chosen TFT does not already include SD hardware |
| 2 | 4-20 mA transmitters | One oil pressure sensor and one oil temperature sensor |
| 2 | 4-20 mA receiver/current-to-voltage modules | Replaces the discrete shunts and RC input filter network |
| 1 | Momentary push button | UI screen switch and fault clear input |
| 1 | Inline fuse and holder | Protects the vehicle-side 12 V feed |
| 1 | Reverse-polarity protection stage | Automotive survivability on the power input |
| 1 | Automotive TVS diode | Load-dump and transient suppression on the 12 V input |
| 1 | 12 V to 5 V buck converter | Supplies the ESP32, TFT, ADC, RTC, and loop interface hardware |
| 1 | Enclosure and wiring set | Connectors, terminals, mounting hardware, and harness materials |

### Integration notes
- The firmware assumes two sensor channels using the calibration ranges in [`include/AppConfig.h`](include/AppConfig.h).
- The default wiring and pin map are documented in [`docs/hardware-setup.md`](docs/hardware-setup.md) and [`include/PinDefinitions.h`](include/PinDefinitions.h).
- If the TFT controller, ESP32 pinout, or storage wiring differs from the defaults, update the hardware definitions before flashing.

### Easier 4-20 mA option
- Instead of building the current-loop receiver front end from discrete shunts, RC filters, and protection parts, you can use two ready-made 4-20 mA receiver/converter modules.
- A practical hobby/prototyping option is the [DFRobot Gravity Analog Current to Voltage Converter](https://www.dfrobot.com/product-1755.html/), one module per sensor channel.
- With receiver modules, the `165 ohm` shunts and the recommended per-channel filter parts are typically no longer needed on your carrier wiring because that analog conditioning moves onto the module.
- If you choose a module-based front end, re-check the channel scaling in [`include/AppConfig.h`](include/AppConfig.h) so the firmware matches the module output range rather than the raw shunt-voltage calculation.

## Project layout
- [`platformio.ini`](platformio.ini)
- [`include/AppConfig.h`](include/AppConfig.h)
- [`src/main.cpp`](src/main.cpp)
- [`docs/hardware-setup.md`](docs/hardware-setup.md)

## Build and flash
1. Install PlatformIO Core or use the PlatformIO VS Code extension.
2. Review the pin mapping in [`include/PinDefinitions.h`](include/PinDefinitions.h) and update it for the actual ESP32-S3 dev board and TFT used.
3. Review sensor ranges, Wi-Fi credentials, and timing values in [`include/AppConfig.h`](include/AppConfig.h).
4. Build and upload with `pio run -t upload`.
5. Open the serial monitor with `pio device monitor`.

## Host-side tests
- Run `./scripts/run-host-tests.sh` to execute hardware-independent logic tests on a desktop machine.
- These tests cover sensor current conversion, threshold faults, engineering-value clamping, filter behavior, RTC/fallback timestamp formatting, and log filename sanitization edge cases.
- GitHub Actions is configured to run the same host test suite on pushes and pull requests in [host-tests.yml](.github/workflows/host-tests.yml).

## Runtime controls
- Short press the UI button to switch between the main gauge screen and the diagnostics screen.
- Hold the UI button for 1.2 seconds to clear latched sensor faults.

## Web endpoints
- `/` simple phone-friendly dashboard mirror
- `/api/live` current readings and system state as JSON
- `/api/files` available CSV files on the SD card
- `/download/<file>` fetch a CSV log file
