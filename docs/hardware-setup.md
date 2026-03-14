# Hardware Setup

## Core modules
- MCU: ESP32-S3 DevKitC-1 class board
- ADC: ADS1115 on I2C
- RTC: DS3231 on I2C with backup cell
- Display: 480x320 SPI TFT using ST7796S
- Storage: microSD on the shared SPI bus
- Sensors: 2x loop-powered 4-20 mA transmitters for oil pressure and oil temperature

## Recommended wiring

### Power front end
- Vehicle 12 V input -> inline fuse -> reverse-polarity protection -> automotive TVS -> buck converter to 5 V
- 5 V rail -> ESP32 5 V/VBUS input, TFT supply, ADS1115 supply, DS3231 supply
- 3.3 V rail from ESP32 -> logic-level pull-ups and any 3.3 V-only interface side
- Sensor supply should come from the protected 12 V rail unless the transmitter datasheet requires higher loop voltage

### Current loop receivers
- Use one 165 ohm 0.1% shunt resistor per channel from sensor current return to ground
- Tap the shunt top node into the ADS1115 channel input through a small series resistor
- Add an RC filter at each ADC input, for example 100 ohm in series and 0.1 uF to ground
- Add clamp protection appropriate for the ADC input domain
- Keep sensor returns star-grounded close to the shunt resistors

### Sensor loop topology
- Protected 12 V -> sensor `+`
- Sensor loop output/current return -> shunt top
- Shunt bottom -> system ground
- ADS1115 A0 -> oil pressure shunt top
- ADS1115 A1 -> oil temperature shunt top
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

Update the values in [`include/PinDefinitions.h`](/Users/james/Documents/Development/motorsport-data-acquisition/include/PinDefinitions.h) if the actual wiring differs.

## BOM guidance
- 1x ESP32-S3 DevKitC-1 or similar
- 1x ST7796S 3.5 inch SPI TFT with 480x320 resolution
- 1x ADS1115 breakout
- 1x DS3231 module
- 1x microSD breakout if not integrated on the TFT
- 2x 165 ohm 0.1% shunt resistors, minimum 0.125 W
- 2x input filter series resistors, 100 ohm nominal
- 2x filter capacitors, 0.1 uF nominal
- Automotive TVS diode, reverse-polarity protection device, buck converter, fuse holder, and enclosure hardware

## Commissioning checklist
1. Confirm the sensor supply voltage and compliance requirement from the actual transmitter datasheets.
2. Verify the shunt voltage at 4 mA and 20 mA before connecting the ADS1115.
3. Confirm the TFT controller is ST7796S. If it is ILI9488 or another controller, update [`include/TFT_Setup.h`](/Users/james/Documents/Development/motorsport-data-acquisition/include/TFT_Setup.h).
4. Set the DS3231 to the correct time before field logging.
5. Inject 4, 8, 12, 16, and 20 mA into each channel and verify the displayed engineering units match the configured ranges.
