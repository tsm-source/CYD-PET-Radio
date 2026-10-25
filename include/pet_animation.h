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

void drawPetClothing(TFT_eSPI &frameBuffer, uint8_t animalIndex,
                     PetOutfit outfit, int16_t bob);
