#if defined(BOARD_FREENOVE_FNK0104B)

#include "es8311_codec.h"

namespace {

constexpr uint8_t ES8311_ADDRESS = 0x18;
constexpr uint8_t DAC_VOLUME_REGISTER_VALUE = 0xBF;

struct RateConfig {
  uint32_t sampleRate;
  uint8_t preMultiplier;
};

// With MCLK = 256 x fs only the pre-multiplier differs between rates.
constexpr RateConfig RATE_CONFIGS[] = {
    {8000, 1},  {11025, 1}, {12000, 1}, {16000, 0}, {22050, 0},
    {24000, 0}, {32000, 0}, {44100, 0}, {48000, 0}};

TwoWire *codecWire = nullptr;

bool writeRegister(uint8_t reg, uint8_t value) {
  codecWire->beginTransmission(ES8311_ADDRESS);
  codecWire->write(reg);
  codecWire->write(value);
  return codecWire->endTransmission() == 0;
}

bool readRegister(uint8_t reg, uint8_t &value) {
  codecWire->beginTransmission(ES8311_ADDRESS);
  codecWire->write(reg);
  if (codecWire->endTransmission(false) != 0 ||
      codecWire->requestFrom(ES8311_ADDRESS, static_cast<uint8_t>(1)) != 1) {
    return false;
  }
  value = codecWire->read();
  return true;
}

}  // namespace

bool es8311SetSampleRate(uint32_t sampleRate) {
  if (codecWire == nullptr) {
    return false;
  }
  for (const RateConfig &config : RATE_CONFIGS) {
    if (config.sampleRate != sampleRate) {
      continue;
    }
    uint8_t reg02 = 0;
    uint8_t reg06 = 0;
    uint8_t reg07 = 0;
    if (!readRegister(0x02, reg02) || !readRegister(0x06, reg06) ||
        !readRegister(0x07, reg07)) {
      return false;
    }
    reg02 = (reg02 & 0x07) | (config.preMultiplier << 3);
    reg06 = (reg06 & 0xE0) | 0x03;
    reg07 &= 0xC0;
    return writeRegister(0x02, reg02) && writeRegister(0x03, 0x10) &&
           writeRegister(0x04, 0x10) && writeRegister(0x05, 0x00) &&
           writeRegister(0x06, reg06) && writeRegister(0x07, reg07) &&
           writeRegister(0x08, 0xFF);
  }
  return false;
}

bool es8311Begin(TwoWire &wire, uint32_t sampleRate) {
  codecWire = &wire;
  uint8_t chipId1 = 0;
  uint8_t chipId2 = 0;
  if (!readRegister(0xFD, chipId1) || !readRegister(0xFE, chipId2)) {
    return false;
  }
  Serial.printf("ES8311 chip id %02X%02X\n", chipId1, chipId2);

  bool ok = writeRegister(0x00, 0x1F);
  delay(20);
  ok = ok && writeRegister(0x00, 0x00) && writeRegister(0x00, 0x80);
  // Clocks on, MCLK taken from the MCLK pin.
  ok = ok && writeRegister(0x01, 0x3F) && es8311SetSampleRate(sampleRate);
  // I2S slave, 16-bit serial data in and out.
  ok = ok && writeRegister(0x09, 0x0C) && writeRegister(0x0A, 0x0C);
  ok = ok && writeRegister(0x0D, 0x01) && writeRegister(0x0E, 0x02) &&
       writeRegister(0x12, 0x00) && writeRegister(0x13, 0x10) &&
       writeRegister(0x1C, 0x6A) && writeRegister(0x37, 0x08) &&
       writeRegister(0x32, DAC_VOLUME_REGISTER_VALUE);
  return ok;
}

#endif
