#pragma once

#include <Arduino.h>
#include "radio_display.h"

#include "pet_animation.h"

struct Pet {
  const char *animalType;
  const char *animalName;
  int8_t drawingIndex;
};

constexpr uint8_t PET_COUNT = 11;

extern const Pet petList[PET_COUNT];

void drawPet(TFT_eSPI &frameBuffer, uint32_t now, const Pet &pet,
             PetDance dance, PetOutfit outfit);
