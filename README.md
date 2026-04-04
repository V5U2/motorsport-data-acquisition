# Motorsport Data Acquisition

ESP32-S3 Arduino/PlatformIO firmware for a configurable 4-20 mA motorsport logger and dashboard.

GitHub releases use Semantic Versioning with `release-please`, and commits should follow Conventional Commits such as `feat:`, `fix:`, `docs:`, `refactor:`, `test:`, `chore:`, `build:`, and `ci:`.

## Features
- Reads a configurable set of 4-20 mA sensors through an ADS1115-based analog front end
- Displays live gauges and diagnostics on a 480x320 SPI TFT
- Logs CSV data to microSD with DS3231 RTC timestamps
- Serves a lightweight Wi-Fi dashboard and CSV download endpoints
- Keeps pin mapping, sensor calibration, and refresh rates in one config file

## Required hardware

This project is documented around a module-first build so the analog front end and power chain use off-the-shelf parts wherever practical.

### Off-the-shelf bill of materials

| Qty | Item | Notes |
| --- | --- | --- |
| 1 | [ESP32-S3 DevKitC-1 class board](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html) | Main controller supported by [`platformio.ini`](platformio.ini) and the default pin map |
| 1 | [3.5 inch 480x320 SPI TFT with ST7796S controller](https://www.waveshare.com/3.5inch-capacitive-touch-lcd.htm) | Main dashboard display; a version with an integrated microSD socket is preferred |
| 1 | [ADS1115 breakout](https://www.adafruit.com/product/1085) | External ADC used to read the sensor-interface module outputs |
| 1 | [DS3231 RTC module with backup coin cell](https://www.adafruit.com/product/5188) | Provides persistent timestamps for CSV logging |
| 1 | [microSD breakout or integrated SD socket](https://www.adafruit.com/product/254) | Needed only if the chosen TFT does not already include SD hardware; removable media is not listed as part of the core system BOM |
| 2 | [4-20 mA receiver/current-to-voltage modules](https://www.dfrobot.com/product-1755.html) | One module per sensor channel; this replaces the custom shunt/filter front end |
| 1 | [Momentary push button](https://www.adafruit.com/product/471) | UI screen switch and fault clear input |
| 1 | [Fused 12 V input path](https://www.bluesea.com/products/5064/) | Inline fuse holder or a prebuilt fused automotive input lead |
| 1 | [12 V reverse-polarity/transient protection module](https://www.pololu.com/product/5380) | Preferred over building the protection stage from discrete parts |
| 1 | [12 V to 5 V buck converter module](https://www.pololu.com/product/2851) | Supplies the ESP32, TFT, ADC, RTC, and loop interface hardware |
| 1 | [Enclosure and wiring set](https://www.polycase.com/general-use-enclosures) | Connectors, terminals, mounting hardware, and harness materials |

### Integration notes
- The firmware sensor list is defined in [`include/AppConfig.h`](include/AppConfig.h), so future channels can be added there without changing the overall project structure.
- The default wiring and pin map are documented in [`docs/hardware-setup.md`](docs/hardware-setup.md) and [`include/PinDefinitions.h`](include/PinDefinitions.h).
- A practical module for each 4-20 mA channel is the [DFRobot Gravity Analog Current to Voltage Converter](https://www.dfrobot.com/product-1755.html).
- External 4-20 mA transmitters are treated as field devices feeding the logger, not part of the logger BOM.
- The logger expects removable microSD media at runtime, but the card itself is treated as consumable media rather than a BOM line item.
- If the TFT controller, ESP32 pinout, or storage wiring differs from the defaults, update the hardware definitions before flashing.
- Re-check the channel scaling in [`include/AppConfig.h`](include/AppConfig.h) so the firmware matches the output range of the chosen 4-20 mA receiver module.

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

## Release process
1. Merge changes into `main` using Conventional Commit-style messages such as `feat:`, `fix:`, or `docs:`.
2. The [`release-please`](.github/workflows/release-please.yml) workflow updates or opens a release PR based on those commits.
3. When that release PR is merged, `release-please` creates the next `v*` tag and GitHub release.
4. The [`release-firmware`](.github/workflows/release.yml) workflow rebuilds the firmware for that tag and attaches the generated binaries to the GitHub release.

## Runtime controls
- Short press the UI button to switch between the main gauge screen and the diagnostics screen.
- Hold the UI button for 1.2 seconds to clear latched sensor faults.

## Web endpoints
- `/` simple phone-friendly dashboard mirror
- `/api/live` current readings and system state as JSON
- `/api/files` available CSV files on the SD card
- `/download/<file>` fetch a CSV log file
