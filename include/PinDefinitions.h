#pragma once

// Central pin definitions shared by the firmware and the TFT_eSPI setup.
// These defaults target the Unexpected Maker TinyS3 plus the optional Shield Logger.

#define PIN_I2C_SDA 8
#define PIN_I2C_SCL 9

#define PIN_SPI_MOSI 35
#define PIN_SPI_MISO 37
#define PIN_SPI_SCLK 36

#define PIN_TFT_CS 3
#define PIN_TFT_DC 4
#define PIN_TFT_RST 5
#define PIN_TFT_BL 6

#define PIN_SD_CS 34

#define PIN_UI_BUTTON 7
