#pragma once

// Central pin definitions shared by the firmware and the TFT_eSPI setup.
// Adjust these values to match the exact ESP32-S3 DevKitC-1 wiring used.

#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

#define PIN_SPI_MOSI 11
#define PIN_SPI_MISO 13
#define PIN_SPI_SCLK 12

#define PIN_TFT_CS 10
#define PIN_TFT_DC 14
#define PIN_TFT_RST 21
#define PIN_TFT_BL 47

#define PIN_SD_CS 15

#define PIN_UI_BUTTON 16
