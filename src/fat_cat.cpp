#include "fat_cat.h"

namespace {

constexpr uint16_t PANEL_COLOUR = 0x0841;
constexpr uint16_t FUR_COLOUR = 0xAD55;
constexpr uint16_t FUR_SHADOW = 0x6B4D;
constexpr uint16_t BELLY_COLOUR = 0xFFDF;
constexpr uint16_t INNER_EAR = 0xF249;
constexpr uint16_t EYE_COLOUR = 0x07E0;

}  // namespace

// Render the built-in cat using the same dance timing and outfit system as the
// other pets, while keeping its dedicated full-panel artwork.
void drawFatCat(TFT_eSPI &frameBuffer, uint32_t now, PetDance dance,
                PetOutfit outfit) {
  const uint8_t pose = (now / FAT_CAT_FRAME_INTERVAL_MS) % 12;
  static const int8_t bounce[] = {0, -2, -5, -8, -10, -8,
                                  -5, -2, 0, 2, 1, 0};
  static const int8_t sway[] = {0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1};
  static const int8_t hops[] = {0, -4, -8, -12, -8, -4,
                                0, -3, -7, -11, -7, -3};
  static const int8_t arms[] = {-7, -12, -17, -21, -17, -12,
                                -7, -12, -17, -21, -17, -12};
  static const int8_t kicks[] = {-4, -2, 2, 6, 2, -2, -4, 2, 6, 2, -2, -4};
  const int16_t bob = dance == PetDance::Sway
                          ? sway[pose]
                          : dance == PetDance::Hop ? hops[pose] : bounce[pose];
  const int16_t step = dance == PetDance::Bounce
                           ? kicks[pose]
                           : dance == PetDance::Sway
                                 ? sway[pose] * 2
                                 : kicks[(pose + 3) % 12];
  const int16_t armLift =
      dance == PetDance::Sway ? arms[(pose + 3) % 12] : arms[pose];
  const int16_t headX = 48 + (dance == PetDance::Sway ? sway[pose] : 0);
  const int16_t headY = 108 + bob;
  const int16_t bodyY = 132 + bob;
  const bool blinking = isPetBlinking(now, 0);

  frameBuffer.fillRect(0, 0, FAT_CAT_PANEL_WIDTH, 195, PANEL_COLOUR);
  frameBuffer.drawFastHLine(0, 193, FAT_CAT_PANEL_WIDTH, 0x3186);
  frameBuffer.fillCircle(15, 44, 1, TFT_YELLOW);
  frameBuffer.fillCircle(77, 105, 1, TFT_YELLOW);

  frameBuffer.drawLine(68, bodyY + 24, 81, bodyY + 17 + bob / 2, FUR_SHADOW);
  frameBuffer.drawLine(81, bodyY + 17 + bob / 2, 86, bodyY + 10 + bob / 2,
                   FUR_SHADOW);
  frameBuffer.fillCircle(86, bodyY + 10 + bob / 2, 3, FUR_COLOUR);

  frameBuffer.fillTriangle(headX - 14, headY - 11, headX - 17, headY - 27,
                           headX - 5, headY - 19, FUR_SHADOW);
  frameBuffer.fillTriangle(headX + 14, headY - 11, headX + 17, headY - 27,
                           headX + 5, headY - 19, FUR_SHADOW);
  frameBuffer.fillTriangle(headX - 13, headY - 15, headX - 14, headY - 23,
                           headX - 8, headY - 18, INNER_EAR);
  frameBuffer.fillTriangle(headX + 13, headY - 15, headX + 14, headY - 23,
                           headX + 8, headY - 18, INNER_EAR);
  frameBuffer.fillRoundRect(25, bodyY, 46, 47, 20, FUR_SHADOW);
  frameBuffer.fillRoundRect(28, bodyY - 2, 40, 44, 18, FUR_COLOUR);
  frameBuffer.fillEllipse(48, bodyY + 23, 14, 17, BELLY_COLOUR);

  frameBuffer.drawLine(32, bodyY + 10, 24, bodyY + armLift, FUR_SHADOW);
  frameBuffer.drawLine(64, bodyY + 10, 72,
                       bodyY + arms[(pose + 6) % 12], FUR_SHADOW);
  frameBuffer.fillCircle(24, bodyY + armLift, 4, BELLY_COLOUR);
  frameBuffer.fillCircle(72, bodyY + arms[(pose + 6) % 12], 4, BELLY_COLOUR);

  frameBuffer.fillCircle(headX, headY, 20, FUR_COLOUR);
  if (blinking) {
    frameBuffer.drawFastHLine(headX - 9, headY + 1, 6, FUR_SHADOW);
    frameBuffer.drawFastHLine(headX + 3, headY + 1, 6, FUR_SHADOW);
  } else {
    frameBuffer.fillCircle(headX - 7, headY + 1, 2, EYE_COLOUR);
    frameBuffer.fillCircle(headX + 7, headY + 1, 2, EYE_COLOUR);
    frameBuffer.fillCircle(headX - 7, headY + 1, 1, TFT_BLACK);
    frameBuffer.fillCircle(headX + 7, headY + 1, 1, TFT_BLACK);
  }
  frameBuffer.fillTriangle(45, headY + 7, 51, headY + 7, 48, headY + 10,
                       INNER_EAR);
  frameBuffer.drawLine(48, headY + 10, 45, headY + 13, TFT_BLACK);
  frameBuffer.drawLine(48, headY + 10, 51, headY + 13, TFT_BLACK);
  frameBuffer.drawLine(35, headY + 9, 28, headY + 8, BELLY_COLOUR);
  frameBuffer.drawLine(35, headY + 12, 28, headY + 13, BELLY_COLOUR);
  frameBuffer.drawLine(61, headY + 9, 68, headY + 8, BELLY_COLOUR);
  frameBuffer.drawLine(61, headY + 12, 68, headY + 13, BELLY_COLOUR);

  frameBuffer.drawLine(37, bodyY + 40, 34 + step, 181 + bob,
                       FUR_SHADOW);
  frameBuffer.drawLine(59, bodyY + 40, 62 - step, 181 - bob,
                       FUR_SHADOW);
  frameBuffer.fillRoundRect(28 + step, 178 + bob, 13, 7, 3,
                            BELLY_COLOUR);
  frameBuffer.fillRoundRect(55 - step, 178 - bob, 13, 7, 3,
                            BELLY_COLOUR);

  drawPetClothing(frameBuffer, 5, outfit, bob);
  if (dance == PetDance::Sway) {
    drawPetMusicNote(frameBuffer, 15 + sway[pose], 90, TFT_CYAN);
    drawPetMusicNote(frameBuffer, 75 - sway[pose], 115, TFT_YELLOW);
  } else if (dance == PetDance::Hop && pose % 3 == 0) {
    drawPetSparkle(frameBuffer, 16, 166, TFT_YELLOW);
    drawPetSparkle(frameBuffer, 84, 159, TFT_CYAN);
  }

  drawPetSparkle(frameBuffer, 17, 80 + bob, TFT_YELLOW);
  drawPetSparkle(frameBuffer, 82, 63 - bob, TFT_YELLOW);
}
