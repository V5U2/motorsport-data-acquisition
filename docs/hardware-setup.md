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

Update the values in [`include/PinDefinitions.h`](../include/PinDefinitions.h) if the actual wiring differs.

## Full BOM

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | ESP32-S3 DevKitC-1 class board | Main controller | Matches the default PlatformIO environment |
| 1 | 3.5 inch 480x320 SPI TFT with ST7796S controller | Local dashboard display | If the controller differs, update [`include/TFT_Setup.h`](../include/TFT_Setup.h) |
| 1 | ADS1115 breakout | Reads the shunt voltages for both 4-20 mA loops | I2C device at address `0x48` by default |
| 1 | DS3231 RTC module with backup cell | Maintains timestamps during power loss | I2C device at address `0x68` by default |
| 1 | microSD card | Stores CSV logs | Size and endurance should match expected logging duration |
| 1 | microSD breakout or integrated SD socket | Connects the card on the shared SPI bus | Optional if the chosen TFT already has SD hardware |
| 2 | 4-20 mA transmitters | Oil pressure and oil temperature measurement | Sensor scaling defaults live in [`include/AppConfig.h`](../include/AppConfig.h) |
| 2 | 165 ohm 0.1% shunt resistors, minimum 0.125 W | Converts loop current to voltage for the ADC | Sized for approximately 0.66 V at 4 mA and 3.30 V at 20 mA |
| 2 | 100 ohm series resistors | Input filtering/protection for ADS1115 channels | Recommended nominal value |
| 2 | 0.1 uF capacitors | RC filter capacitors at the ADC inputs | Recommended nominal value |
| 1 | Momentary push button | UI mode toggle and latched fault clear | Connect to the configured button input |
| 1 | Inline fuse and holder | Protects the incoming 12 V feed | Size for the complete assembly current draw |
| 1 | Reverse-polarity protection stage | Prevents damage from supply reversal | MOSFET or diode-based implementation |
| 1 | Automotive TVS diode | Handles input transients and load dump events | Select for the vehicle electrical system |
| 1 | 12 V to 5 V buck converter | Generates the regulated supply rail | Should comfortably cover TFT backlight and SD peaks |
| 1 | Enclosure, connectors, and wiring hardware | Physical integration | Includes harness, terminals, mounting hardware, and grounding hardware |

## Simplified BOM

If you want to reduce the analog front-end build effort, use one ready-made current-loop receiver module per sensor channel instead of the discrete shunt/filter network.

| Qty | Item | Purpose | Notes |
| --- | --- | --- | --- |
| 1 | ESP32-S3 DevKitC-1 class board | Main controller | Matches the default PlatformIO environment |
| 1 | 3.5 inch 480x320 SPI TFT with ST7796S controller | Local dashboard display | If the controller differs, update [`include/TFT_Setup.h`](../include/TFT_Setup.h) |
| 2 | 4-20 mA receiver/current-to-voltage modules | Converts each loop signal into a board-friendly voltage | One module per sensor channel |
| 1 | ADS1115 breakout | Reads the module voltage outputs | Keep if the receiver module is analog-output only |
| 1 | DS3231 RTC module with backup cell | Maintains timestamps during power loss | I2C device at address `0x68` by default |
| 1 | microSD card | Stores CSV logs | Size and endurance should match expected logging duration |
| 1 | microSD breakout or integrated SD socket | Connects the card on the shared SPI bus | Optional if the chosen TFT already has SD hardware |
| 2 | 4-20 mA transmitters | Oil pressure and oil temperature measurement | Sensor scaling defaults live in [`include/AppConfig.h`](../include/AppConfig.h) |
| 1 | Momentary push button | UI mode toggle and latched fault clear | Connect to the configured button input |
| 1 | Inline fuse and holder | Protects the incoming 12 V feed | Size for the complete assembly current draw |
| 1 | Reverse-polarity protection stage | Prevents damage from supply reversal | MOSFET or diode-based implementation |
| 1 | Automotive TVS diode | Handles input transients and load dump events | Select for the vehicle electrical system |
| 1 | 12 V to 5 V buck converter | Generates the regulated supply rail | Should comfortably cover TFT backlight and SD peaks |
| 1 | Enclosure, connectors, and wiring hardware | Physical integration | Includes harness, terminals, mounting hardware, and grounding hardware |

Recommended example module:
- [DFRobot Gravity Analog Current to Voltage Converter](https://www.dfrobot.com/product-1755.html/) for a simple breakout-style implementation.

When using receiver modules:
- You can usually omit the `165 ohm` shunt resistors and the per-channel `100 ohm` plus `0.1 uF` filter network from the discrete BOM.
- Confirm the module output range before wiring it to the ADS1115 or any direct ESP32 ADC input.
- Update the engineering conversion assumptions in [`include/AppConfig.h`](../include/AppConfig.h) if the module output scaling no longer matches the original shunt-based design.

## Commissioning checklist
1. Confirm the sensor supply voltage and compliance requirement from the actual transmitter datasheets.
2. Verify the shunt voltage at 4 mA and 20 mA before connecting the ADS1115.
3. Confirm the TFT controller is ST7796S. If it is ILI9488 or another controller, update [`include/TFT_Setup.h`](../include/TFT_Setup.h).
4. Set the DS3231 to the correct time before field logging.
5. Inject 4, 8, 12, 16, and 20 mA into each channel and verify the displayed engineering units match the configured ranges.
