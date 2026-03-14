# Motorsport Data Acquisition

ESP32-S3 Arduino/PlatformIO firmware for a two-channel 4-20 mA motorsport logger and dashboard.

## Features
- Reads oil pressure and oil temperature from 4-20 mA sensors through an ADS1115 and 165 ohm shunts
- Displays live gauges and diagnostics on a 480x320 SPI TFT
- Logs CSV data to microSD with DS3231 RTC timestamps
- Serves a lightweight Wi-Fi dashboard and CSV download endpoints
- Keeps pin mapping, sensor calibration, and refresh rates in one config file

## Project layout
- [`platformio.ini`](/Users/james/Documents/Development/motorsport-data-acquisition/platformio.ini)
- [`include/AppConfig.h`](/Users/james/Documents/Development/motorsport-data-acquisition/include/AppConfig.h)
- [`src/main.cpp`](/Users/james/Documents/Development/motorsport-data-acquisition/src/main.cpp)
- [`docs/hardware-setup.md`](/Users/james/Documents/Development/motorsport-data-acquisition/docs/hardware-setup.md)

## Build and flash
1. Install PlatformIO Core or use the PlatformIO VS Code extension.
2. Review the pin mapping in [`include/PinDefinitions.h`](/Users/james/Documents/Development/motorsport-data-acquisition/include/PinDefinitions.h) and update it for the actual ESP32-S3 dev board and TFT used.
3. Review sensor ranges, Wi-Fi credentials, and timing values in [`include/AppConfig.h`](/Users/james/Documents/Development/motorsport-data-acquisition/include/AppConfig.h).
4. Build and upload with `pio run -t upload`.
5. Open the serial monitor with `pio device monitor`.

## Host-side tests
- Run `./scripts/run-host-tests.sh` to execute hardware-independent logic tests on a desktop machine.
- These tests cover sensor current conversion, threshold faults, engineering-value clamping, filter behavior, RTC/fallback timestamp formatting, and log filename sanitization edge cases.
- GitHub Actions is configured to run the same host test suite on pushes and pull requests in [host-tests.yml](/Users/james/Documents/Development/motorsport-data-acquisition/.github/workflows/host-tests.yml).

## Runtime controls
- Short press the UI button to switch between the main gauge screen and the diagnostics screen.
- Hold the UI button for 1.2 seconds to clear latched sensor faults.

## Web endpoints
- `/` simple phone-friendly dashboard mirror
- `/api/live` current readings and system state as JSON
- `/api/files` available CSV files on the SD card
- `/download/<file>` fetch a CSV log file
