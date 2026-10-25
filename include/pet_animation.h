#pragma once

#include <Arduino.h>
#include "radio_display.h"

enum class PetDance : uint8_t {
  Bounce,
  Sway,
  Hop
};

enum class PetOutfit : uint8_t {
  Tie,
  Sunglasses,
  Boots,
  GlitterJacket
};

inline bool isPetBlinking(uint32_t now, uint8_t petIndex) {
  constexpr uint32_t BLINK_PERIOD_MS = 5000;
  constexpr uint32_t BLINK_DURATION_MS = 160;
  const uint32_t phase = (now + petIndex * 379U) % BLINK_PERIOD_MS;
  return phase < BLINK_DURATION_MS;
}

inline void drawPetMusicNote(TFT_eSPI &frameBuffer, int16_t x, int16_t y,
                             uint16_t colour) {
  frameBuffer.fillCircle(x + 2, y + 11, 3, colour);
  frameBuffer.drawFastVLine(x + 4, y, 12, colour);
  frameBuffer.drawLine(x + 4, y, x + 10, y + 3, colour);
}

inline void drawPetSparkle(TFT_eSPI &frameBuffer, int16_t x, int16_t y,
                           uint16_t colour) {
  frameBuffer.drawFastHLine(x - 4, y, 9, colour);
  frameBuffer.drawFastVLine(x, y - 4, 9, colour);
  frameBuffer.drawLine(x - 3, y - 3, x + 3, y + 3, colour);
  frameBuffer.drawLine(x + 3, y - 3, x - 3, y + 3, colour);
}

void drawPetClothing(TFT_eSPI &frameBuffer, uint8_t animalIndex,
                     PetOutfit outfit, int16_t bob);
