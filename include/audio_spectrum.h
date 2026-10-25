#pragma once

#include <Arduino.h>
#include "radio_display.h"

void setAudioSpectrumEnabled(bool enabled);
void drawAudioSpectrum(TFT_eSPI &frameBuffer, uint32_t now,
                       bool streamActive, bool enabled);
