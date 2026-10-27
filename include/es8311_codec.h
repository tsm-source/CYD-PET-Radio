#pragma once

#include <Arduino.h>
#include <Wire.h>

// Minimal ES8311 DAC driver (I2S slave, MCLK = 256 x sample rate).
// Register values follow Espressif's Apache-2.0 es8311 driver.
bool es8311Begin(TwoWire &wire, uint32_t sampleRate);
bool es8311SetSampleRate(uint32_t sampleRate);
