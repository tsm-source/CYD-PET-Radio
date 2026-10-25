#pragma once

#include <Arduino.h>
#include "radio_display.h"

#include "pet_animation.h"

constexpr int16_t FAT_CAT_PANEL_WIDTH = 96;
#if defined(BOARD_JC3248W535)
constexpr uint16_t FAT_CAT_FRAME_INTERVAL_MS = 67;
#else
constexpr uint16_t FAT_CAT_FRAME_INTERVAL_MS = 100;
#endif

void drawFatCat(TFT_eSPI &frameBuffer, uint32_t now,
                PetDance dance = PetDance::Bounce,
                PetOutfit outfit = PetOutfit::Tie);
