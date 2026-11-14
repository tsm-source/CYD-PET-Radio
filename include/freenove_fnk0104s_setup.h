#pragma once

#define USER_SETUP_INFO "Freenove FNK0104S 4.0 inch ST7796"
#define ST7796_DRIVER
#define TFT_WIDTH 320
#define TFT_HEIGHT 480
#define TFT_ROTATION 1

#define USE_HSPI_PORT
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS 10
#define TFT_DC 46
#define TFT_RST -1
#define TFT_BL 45
#define TFT_BACKLIGHT_ON HIGH

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 80000000
#endif

#define LOAD_GLCD
#define LOAD_FONT2

#define TFT_INVERSION_ON
