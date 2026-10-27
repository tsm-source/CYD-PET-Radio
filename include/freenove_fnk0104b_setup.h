#pragma once

#define USER_SETUP_INFO "Freenove FNK0104B 2.8 inch ILI9341"
#define ILI9341_2_DRIVER
#define TFT_WIDTH 240
#define TFT_HEIGHT 320
#define TFT_ROTATION 1

#define USE_HSPI_PORT
#define TFT_MISO 13
#define TFT_MOSI 11
#define TFT_SCLK 12
#define TFT_CS 10
#define TFT_DC 46
#define TFT_RST -1
#define TFT_BL 45
#define TFT_BACKLIGHT_ON HIGH

#ifndef SPI_FREQUENCY
#define SPI_FREQUENCY 40000000
#endif
#define SPI_READ_FREQUENCY 20000000

#define LOAD_GLCD
#define LOAD_FONT2

#define TFT_INVERSION_ON
#define TFT_RGB_ORDER TFT_BGR
