#pragma once

// Central pin definitions shared by the firmware and the TFT_eSPI setup.
// These defaults target a NodeMCU 1.0 / ESP-12E DevKit V2. GPIO numbers are
// used in code; the matching NodeMCU D-labels are included for wiring.

#define PIN_I2C_SDA 4   // D2
#define PIN_I2C_SCL 5   // D1

#define MDA_PIN_SPI_MOSI 13  // D7
#define MDA_PIN_SPI_MISO 12  // D6
#define MDA_PIN_SPI_SCLK 14  // D5

#define PIN_TFT_CS 15    // D8; must remain low during boot
#define PIN_TFT_DC 0     // D3; must remain high during boot
#define PIN_TFT_RST 2    // D4; must remain high during boot
#define PIN_TFT_BL -1    // Wire backlight to the appropriate supply

#define PIN_SD_CS 16     // D0

#define PIN_UI_BUTTON 3  // RX; firmware only transmits serial diagnostics
#define PIN_STATUS_LED 2 // D4; NodeMCU built-in LED, active low
