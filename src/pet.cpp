#include <math.h>

#include "fat_cat.h"
#include "pets.h"

namespace {

enum PetDrawingIndex : uint8_t {
  PeacockPet,
  LionPet,
  KangarooPet,
  ChameleonPet,
  SwanPet,
  MoritzPet,
  RhinoPet,
  WilliPet,
  BlitzPet,
  HansiPet
};

void drawFrame(TFT_eSPI &frameBuffer) {
  frameBuffer.fillRect(0, 0, FAT_CAT_PANEL_WIDTH, 193, 0x0841);
  frameBuffer.drawFastHLine(0, 193, FAT_CAT_PANEL_WIDTH, 0x3186);
  frameBuffer.fillCircle(15, 44, 1, TFT_YELLOW);
  frameBuffer.fillCircle(77, 105, 1, TFT_YELLOW);
}

uint16_t rgb(uint8_t red, uint8_t green, uint8_t blue) {
  return ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3);
}

// Draw a line with rounded ends using two caps and a pair of edge triangles.
void thickLine(TFT_eSPI &frame, int16_t x0, int16_t y0, int16_t x1,
               int16_t y1, int16_t width, uint16_t colour) {
  const float dx = x1 - x0;
  const float dy = y1 - y0;
  const float length = sqrtf(dx * dx + dy * dy);
  frame.fillCircle(x0, y0, width / 2, colour);
  frame.fillCircle(x1, y1, width / 2, colour);
  if (length < 1.0f) {
    return;
  }
  const int16_t nx = lroundf(-dy / length * width / 2.0f);
  const int16_t ny = lroundf(dx / length * width / 2.0f);
  frame.fillTriangle(x0 + nx, y0 + ny, x0 - nx, y0 - ny, x1 + nx, y1 + ny,
                     colour);
  frame.fillTriangle(x0 - nx, y0 - ny, x1 - nx, y1 - ny, x1 + nx, y1 + ny,
                     colour);
}

void drawClaws(TFT_eSPI &frame, int16_t x, int16_t y, int16_t direction) {
  const int16_t top = direction > 0 ? y : y - 7;
  for (int16_t offset = -3; offset <= 3; offset += 3) {
    frame.fillRect(x + offset, top, 2, 7, rgb(240, 235, 215));
  }
}

void drawSlothFace(TFT_eSPI &frame, int16_t cx, int16_t cy, bool blinking) {
  const uint16_t mask = rgb(60, 40, 30);
  const uint16_t dark = rgb(30, 20, 20);
  frame.fillEllipse(cx - 8, cy - 2, 6, 5, mask);
  frame.fillEllipse(cx + 8, cy - 2, 6, 5, mask);
  if (blinking) {
    frame.drawFastHLine(cx - 11, cy - 2, 7, rgb(232, 216, 172));
    frame.drawFastHLine(cx + 5, cy - 2, 7, rgb(232, 216, 172));
  } else {
    frame.fillCircle(cx - 8, cy - 2, 2, TFT_WHITE);
    frame.fillCircle(cx + 8, cy - 2, 2, TFT_WHITE);
    frame.fillCircle(cx - 8, cy - 2, 1, TFT_BLACK);
    frame.fillCircle(cx + 8, cy - 2, 1, TFT_BLACK);
  }
  frame.fillEllipse(cx, cy + 5, 3, 2, dark);
  frame.drawLine(cx - 5, cy + 8, cx - 2, cy + 10, dark);
  frame.drawLine(cx - 2, cy + 10, cx + 2, cy + 10, dark);
  frame.drawLine(cx + 2, cy + 10, cx + 5, cy + 8, dark);
}

// Willi hanging from a branch and swinging.
void drawWilliHanging(TFT_eSPI &frame, int16_t swing, int16_t bob,
                      uint8_t pose, bool blinking) {
  const uint16_t branch = rgb(110, 70, 35);
  const uint16_t leaf = rgb(60, 170, 70);
  const uint16_t fur = rgb(150, 135, 85);
  const uint16_t shade = rgb(105, 90, 55);
  const int16_t cx = 48 + swing;
  const int16_t bodyY = 130 + bob;
  const int16_t headY = 98 + bob;

  frame.fillRoundRect(2, 62, 92, 7, 3, branch);
  frame.fillEllipse(10, 58, 8, 3, leaf);
  frame.fillEllipse(84, 58, 8, 3, leaf);
  frame.fillEllipse(48, 57, 8, 3, leaf);
  thickLine(frame, cx - 12, bodyY - 14, 28, 66, 5, shade);
  thickLine(frame, cx + 12, bodyY - 14, 68, 66, 5, shade);
  drawClaws(frame, 28, 62, 1);
  drawClaws(frame, 68, 62, 1);
  thickLine(frame, cx - 8, bodyY + 20, 38 - swing, 172, 5, shade);
  thickLine(frame, cx + 8, bodyY + 20, 58 - swing, 172, 5, shade);
  drawClaws(frame, 38 - swing, 170, 1);
  drawClaws(frame, 58 - swing, 170, 1);
  frame.fillEllipse(cx, bodyY, 22, 29, fur);
  frame.fillEllipse(cx, bodyY + 6, 13, 18, rgb(190, 170, 120));
  frame.fillEllipse(cx, headY, 20, 17, fur);
  frame.fillEllipse(cx, headY + 4, 16, 13, rgb(232, 216, 172));
  drawSlothFace(frame, cx, headY + 1, blinking);
}

// Willi sitting upright, waving and wearing a rasta cap.
void drawWilliSitting(TFT_eSPI &frame, int16_t bob, uint8_t pose,
                      bool blinking) {
  static const int8_t wave[] = {0, 2, 4, 6, 4, 2, 0, -2, -4, -6, -4, -2};
  const uint16_t fur = rgb(150, 135, 85);
  const uint16_t shade = rgb(105, 90, 55);
  const int16_t bodyY = 152 + bob;
  const int16_t headY = 102 + bob;
  const int16_t footY = 181 + bob / 3;
  const int16_t handX = 14 + wave[pose];
  const int16_t handY = 116 + bob;

  frame.fillEllipse(48, bodyY, 27, 29, fur);
  frame.fillEllipse(48, bodyY + 6, 16, 19, rgb(190, 170, 120));
  frame.fillEllipse(30, footY, 10, 6, shade);
  frame.fillEllipse(66, footY, 10, 6, shade);
  drawClaws(frame, 24, footY, 1);
  drawClaws(frame, 60, footY, 1);
  thickLine(frame, 26, bodyY - 12, handX, handY, 6, shade);
  frame.fillCircle(handX, handY - 2, 4, shade);
  drawClaws(frame, handX, handY - 8, -1);
  thickLine(frame, 70, bodyY - 12, 76, bodyY + 12, 6, shade);
  frame.fillCircle(76, bodyY + 14, 4, shade);
  frame.fillEllipse(48, headY, 22, 20, fur);
  frame.fillEllipse(48, headY + 3, 17, 14, rgb(232, 216, 172));
  frame.fillEllipse(48, headY - 18, 23, 14, rgb(30, 150, 60));
  frame.fillEllipse(48, headY - 20, 21, 11, rgb(250, 210, 40));
  frame.fillEllipse(48, headY - 22, 19, 8, rgb(220, 40, 40));
  drawSlothFace(frame, 48, headY, blinking);
}

// Egon on a branch; the neon variant has headphones and animated stripes.
void drawChameleon(TFT_eSPI &frame, int16_t bob, uint8_t pose, bool neon,
                   bool blinking) {
  constexpr int16_t BRANCH_SHIFT = 26;
  const uint16_t green = rgb(30, 215, 150);
  const uint16_t darkGreen = rgb(20, 150, 105);
  const uint16_t light = rgb(170, 255, 215);
  const uint16_t magenta = rgb(225, 60, 225);
  const uint16_t yellow = rgb(255, 230, 60);
  const uint16_t blue = rgb(60, 140, 255);
  const uint16_t cycle[3] = {magenta, yellow, blue};
  const uint16_t darkBlue = rgb(40, 40, 60);
  const int16_t footY = 153 + BRANCH_SHIFT;
  const int16_t bodyX = 44;
  const int16_t bodyY = 128 + BRANCH_SHIFT + bob;
  const int16_t footShift = pose % 6 < 3 ? 2 : -2;
  const int16_t headX = 72;
  const int16_t headY = 122 + BRANCH_SHIFT + bob;

  frame.fillRoundRect(4, 152 + BRANCH_SHIFT, 88, 7, 3, rgb(110, 70, 35));
  frame.fillEllipse(12, 150 + BRANCH_SHIFT, 8, 3, rgb(60, 170, 70));
  frame.fillEllipse(80, 150 + BRANCH_SHIFT, 8, 3, rgb(60, 170, 70));

  const float curl = 0.3f + sinf(pose * 0.5236f) * 0.25f;
  for (float t = 0.0f; t < 13.2f; t += 0.2f) {
    const float radius = 13.0f - t * 0.85f;
    frame.fillCircle(lroundf(22 + radius * cosf(curl + t)),
                     lroundf(bodyY + 6 + radius * sinf(curl + t)), 2,
                     darkGreen);
  }
  thickLine(frame, 56, bodyY + 12, 60 - footShift, footY, 3, darkGreen);
  thickLine(frame, 28, bodyY + 12, 24 - footShift, footY, 3, darkGreen);

  frame.fillEllipse(bodyX, bodyY, 26, 17, green);
  if (neon) {
    for (int16_t column = -26; column <= 26; ++column) {
      const int16_t offset = column + 26 + pose * 2;
      if (offset % 6 >= 3) {
        continue;
      }
      const float ratio = static_cast<float>(column) / 26.0f;
      const int16_t half = lroundf(17.0f * sqrtf(1.0f - ratio * ratio));
      frame.drawFastVLine(bodyX + column, bodyY - half, 2 * half + 1,
                          cycle[(offset / 6) % 3]);
    }
  } else {
    const uint8_t shift = pose / 4;
    frame.fillEllipse(bodyX, bodyY + 9, 21, 7, light);
    frame.fillCircle(32, bodyY - 6, 4, cycle[shift % 3]);
    frame.fillCircle(44, bodyY - 10, 3, cycle[(shift + 1) % 3]);
    frame.fillCircle(54, bodyY - 4, 4, cycle[(shift + 2) % 3]);
    frame.fillCircle(38, bodyY + 4, 3, cycle[(shift + 1) % 3]);
    frame.fillCircle(50, bodyY + 5, 3, cycle[shift % 3]);
  }
  for (int16_t spike = 0; spike < 7; ++spike) {
    const float x = bodyX - 22 + spike * 7.3f;
    const float ratio = (x - bodyX) / 26.0f;
    const int16_t top = lroundf(bodyY - 17.0f * sqrtf(1.0f - ratio * ratio));
    frame.fillTriangle(lroundf(x) - 3, top + 1, lroundf(x), top - 6,
                       lroundf(x) + 3, top + 1, darkGreen);
  }

  thickLine(frame, 62, bodyY + 14, 66 + footShift, footY, 4, green);
  thickLine(frame, 32, bodyY + 14, 28 + footShift, footY, 4, green);
  for (int16_t foot : {66 + footShift, 28 + footShift}) {
    frame.drawLine(foot, footY, foot + 4, footY - 2, green);
    frame.drawLine(foot, footY, foot - 4, footY - 2, green);
  }

  static const int8_t pupilX[] = {5, 6, 7, 6};
  frame.fillTriangle(headX - 12, headY - 6, headX - 16, headY - 22,
                     headX + 2, headY - 10, darkGreen);
  frame.fillEllipse(headX, headY, 14, 11, green);
  frame.fillEllipse(headX + 10, headY + 3, 7, 5, green);
  frame.drawLine(headX + 17, headY + 6, headX + 2, headY + 8,
                 rgb(10, 70, 50));
  frame.fillCircle(headX + 5, headY - 3, 7, rgb(15, 110, 80));
  if (blinking) {
    frame.fillEllipse(headX + 5, headY - 3, 5, 4, green);
    frame.drawFastHLine(headX, headY - 3, 10, darkGreen);
  } else {
    frame.fillCircle(headX + 5, headY - 3, 5, TFT_WHITE);
    frame.fillCircle(headX + pupilX[pose / 3], headY - 3, 3, TFT_BLACK);
  }
  frame.fillCircle(headX + 15, headY + 1, 1, rgb(10, 70, 50));

  if (neon) {
    for (int16_t angle = 190; angle <= 345; angle += 8) {
      const float radians = angle * 0.0174533f;
      frame.fillCircle(lroundf(headX + 15 * cosf(radians)),
                       lroundf(headY - 2 + 17 * sinf(radians)), 2, darkBlue);
    }
    frame.fillCircle(headX - 6, headY + 2, 6, darkBlue);
    frame.fillCircle(headX - 6, headY + 2, pose % 6 < 3 ? 4 : 3, magenta);
  }
}

constexpr int8_t kWave[] = {0, 2, 4, 6, 4, 2, 0, -2, -4, -6, -4, -2};

// Fill a four-corner polygon as two triangles using the display primitives.
void fillQuad(TFT_eSPI &frame, int16_t x0, int16_t y0, int16_t x1,
              int16_t y1, int16_t x2, int16_t y2, int16_t x3, int16_t y3,
              uint16_t colour) {
  frame.fillTriangle(x0, y0, x1, y1, x2, y2, colour);
  frame.fillTriangle(x0, y0, x2, y2, x3, y3, colour);
}

// Approximate an elliptical thick arc with overlapping circles.
void drawThickArc(TFT_eSPI &frame, int16_t cx, int16_t cy, int16_t rx,
                  int16_t ry, int16_t from, int16_t to, int16_t width,
                  uint16_t colour) {
  for (int16_t angle = from; angle <= to; angle += 4) {
    const float radians = angle * 0.0174533f;
    frame.fillCircle(cx + lroundf(rx * cosf(radians)),
                     cy + lroundf(ry * sinf(radians)), width / 2, colour);
  }
}

// Hoppe the kangaroo: hoodie with peace sign, or headphones with boombox.
void drawKangaroo(TFT_eSPI &frame, int16_t b, uint8_t pose, bool boombox,
                  bool blinking) {
  const uint16_t fur = rgb(205, 150, 95);
  const uint16_t dark = rgb(160, 110, 65);
  const uint16_t light = rgb(238, 208, 165);
  const uint16_t pink = rgb(230, 150, 150);
  const int16_t feetY = 176 + b / 2;

  thickLine(frame, 58, 152 + b, 86, feetY, 7, dark);
  for (int16_t fx = 36; fx <= 60; fx += 24) {
    frame.fillEllipse(fx, 156 + b, 11, 16, fur);
    frame.fillRoundRect(fx - 12, feetY, 24, 8, 4, dark);
  }
  frame.fillEllipse(48, 135 + b, 19, 29, fur);
  frame.fillEllipse(48, 142 + b, 12, 19, light);
  for (int16_t sx = -1; sx <= 1; sx += 2) {
    frame.fillTriangle(48 + sx * 12, 92 + b, 48 + sx * 15, 64 + b,
                       48 + sx * 5, 86 + b, fur);
    frame.fillTriangle(48 + sx * 11, 88 + b, 48 + sx * 14, 70 + b,
                       48 + sx * 7, 86 + b, pink);
  }

  if (!boombox) {
    const uint16_t grey = rgb(90, 90, 110);
    const int16_t wave = kWave[pose] / 2;
    frame.fillEllipse(48, 99 + b, 16, 17, grey);
    frame.fillEllipse(48, 99 + b, 12, 14, fur);
    frame.fillEllipse(48, 138 + b, 21, 27, grey);
    frame.fillRoundRect(36, 146 + b, 24, 11, 4, rgb(70, 70, 90));
    frame.drawLine(42, 121 + b, 40, 140 + b, rgb(210, 210, 230));
    frame.drawLine(54, 121 + b, 56, 140 + b, rgb(210, 210, 230));
    thickLine(frame, 29, 124 + b, 22, 142 + b, 6, grey);
    frame.fillCircle(22, 143 + b, 3, fur);
    thickLine(frame, 67, 124 + b, 76 + wave, 104 + b, 6, grey);
    frame.fillCircle(77 + wave, 101 + b, 3, fur);
    thickLine(frame, 76 + wave, 99 + b, 74 + wave, 92 + b, 2, fur);
    thickLine(frame, 78 + wave, 99 + b, 80 + wave, 92 + b, 2, fur);
  }

  frame.fillEllipse(48, 107 + b, 8, 7, light);
  frame.fillCircle(48, 104 + b, 3, pink);
  if (blinking) {
    frame.drawFastHLine(39, 96 + b, 8, dark);
    frame.drawFastHLine(49, 96 + b, 8, dark);
  } else {
    frame.fillCircle(43, 96 + b, 3, TFT_WHITE);
    frame.fillCircle(53, 96 + b, 3, TFT_WHITE);
    frame.fillCircle(43, 96 + b, 1, TFT_BLACK);
    frame.fillCircle(53, 96 + b, 1, TFT_BLACK);
  }
  frame.drawFastVLine(48, 107 + b, 3, dark);
  frame.drawFastHLine(45, 111 + b, 7, dark);

  if (boombox) {
    const uint16_t phones = rgb(40, 40, 50);
    const bool beat = pose % 6 < 3;
    drawThickArc(frame, 48, 98 + b, 16, 17, 180, 360, 3, phones);
    frame.fillCircle(32, 102 + b, 5, phones);
    frame.fillCircle(64, 102 + b, 5, phones);
    frame.fillCircle(32, 102 + b, 2, beat ? rgb(0, 200, 255) : rgb(0, 110, 190));
    frame.fillCircle(64, 102 + b, 2, beat ? rgb(0, 200, 255) : rgb(0, 110, 190));
    thickLine(frame, 31, 120 + b, 22, 136 + b, 5, fur);
    frame.fillRoundRect(54, 118 + b, 34, 17, 3, rgb(60, 60, 70));
    drawThickArc(frame, 71, 118 + b, 7, 6, 180, 360, 2, rgb(150, 150, 160));
    for (int16_t sx = 63; sx <= 79; sx += 16) {
      frame.fillCircle(sx, 127 + b, beat ? 6 : 5, rgb(30, 30, 30));
      frame.fillCircle(sx, 127 + b, beat ? 3 : 2, rgb(0, 220, 120));
    }
    thickLine(frame, 65, 123 + b, 70, 130 + b, 5, fur);
  }
}

// Siegfried the rhino standing upright: with a flying-V guitar or devil horns.
void drawRhino(TFT_eSPI &frame, int16_t b, uint8_t pose, bool devil,
               bool blinking) {
  static const int8_t strum[] = {0, 4, 8, 4};
  const uint16_t grey = rgb(150, 150, 160);
  const uint16_t dark = rgb(110, 110, 120);
  const uint16_t light = rgb(190, 190, 200);
  const uint16_t horn = rgb(240, 235, 215);
  const uint16_t leather = rgb(25, 25, 30);
  const uint16_t stud = rgb(220, 220, 230);
  const uint16_t red = rgb(200, 40, 40);
  const int16_t feetY = 176 + b / 2;

  thickLine(frame, 62, 160 + b, 88, feetY, 5, dark);
  for (int16_t fx = 36; fx <= 60; fx += 24) {
    frame.fillEllipse(fx, 160 + b, 11, 17, dark);
    frame.fillRoundRect(fx - 12, feetY, 24, 8, 4, dark);
  }
  frame.fillEllipse(48, 135 + b, 24, 29, grey);
  frame.fillEllipse(48, 142 + b, 14, 18, light);
  frame.fillRoundRect(26, 118 + b, 44, 14, 6, leather);
  for (int16_t sx = 30; sx < 68; sx += 8) {
    frame.fillCircle(sx, 125 + b, 1, stud);
  }
  for (int16_t sx = -1; sx <= 1; sx += 2) {
    frame.fillTriangle(48 + sx * 14, 90 + b, 48 + sx * 20, 74 + b,
                       48 + sx * 6, 84 + b, grey);
  }
  frame.fillEllipse(48, 100 + b, 16, 15, light);
  frame.fillTriangle(44, 92 + b, 48, 66 + b, 52, 92 + b, horn);
  frame.fillTriangle(46, 88 + b, 48, 78 + b, 50, 88 + b, rgb(215, 205, 180));
  if (blinking) {
    frame.drawFastHLine(37, 98 + b, 7, dark);
    frame.drawFastHLine(53, 98 + b, 7, dark);
  } else {
    frame.fillCircle(40, 98 + b, 2, TFT_BLACK);
    frame.fillCircle(56, 98 + b, 2, TFT_BLACK);
    frame.drawLine(36, 94 + b, 44, 96 + b, TFT_BLACK);
    frame.drawLine(60, 94 + b, 52, 96 + b, TFT_BLACK);
  }
  frame.fillCircle(43, 109 + b, 1, dark);
  frame.fillCircle(53, 109 + b, 1, dark);
  frame.drawFastHLine(42, 114 + b, 12, rgb(60, 20, 20));

  if (!devil) {
    const int16_t hand = strum[pose % 4];
    fillQuad(frame, 14, 132 + b, 30, 150 + b, 26, 156 + b, 10, 138 + b, red);
    fillQuad(frame, 50, 150 + b, 86, 114 + b, 92, 120 + b, 58, 160 + b, red);
    fillQuad(frame, 30, 154 + b, 64, 156 + b, 80, 128 + b, 86, 134 + b,
             rgb(60, 60, 60));
    thickLine(frame, 80, 116 + b, 72, 100 + b, 4, rgb(70, 50, 30));
    fillQuad(frame, 26, 150 + b, 50, 142 + b, 68, 158 + b, 44, 164 + b, red);
    thickLine(frame, 32, 124 + b, 40, 148 + b, 6, grey);
    thickLine(frame, 64, 124 + b, 78, 118 + b + hand, 6, grey);
    frame.fillCircle(32, 148 + b, 2, stud);
    frame.fillCircle(40, 146 + b, 2, stud);
  } else {
    const int16_t lift = kWave[pose] / 2;
    const uint16_t hair = rgb(20, 20, 25);
    frame.fillTriangle(22, 90 + b, 16, 130 + b, 28, 130 + b, hair);
    frame.fillTriangle(74, 90 + b, 80, 130 + b, 68, 130 + b, hair);
    frame.fillEllipse(48, 86 + b, 16, 6, hair);
    frame.fillTriangle(36, 90 + b, 40, 94 + b, 36, 96 + b, red);
    frame.drawFastHLine(44, 76 + b, 8, red);
    thickLine(frame, 31, 126 + b, 20, 108 + b + lift, 6, grey);
    frame.fillCircle(20, 106 + b + lift, 4, grey);
    frame.fillTriangle(17, 103 + b + lift, 15, 94 + b + lift, 20,
                       101 + b + lift, grey);
    frame.fillTriangle(22, 101 + b + lift, 25, 93 + b + lift, 23,
                       104 + b + lift, grey);
    thickLine(frame, 65, 126 + b, 70, 144 + b, 6, grey);
  }
}

// Moritz the jazz cat with a beret and saxophone.
void drawMoritz(TFT_eSPI &frame, int16_t b, uint8_t pose, bool blinking) {
  const uint16_t fur = rgb(45, 45, 55);
  const uint16_t belly = rgb(230, 230, 235);
  const uint16_t pink = rgb(235, 150, 160);
  const uint16_t gold = rgb(240, 190, 40);
  const uint16_t whisker = rgb(225, 225, 225);
  const int16_t hy = 84 + b;
  const int16_t tap = pose % 4 < 2 ? 0 : 3;

  thickLine(frame, 72, 168, 88, 150 + b, 5, fur);
  thickLine(frame, 88, 150 + b, 82, 138 + b - kWave[pose] / 2, 5, fur);
  frame.fillEllipse(48, 150 + b, 24, 30, fur);
  frame.fillEllipse(48, 156 + b, 14, 21, belly);
  frame.fillEllipse(36, 177, 8, 5, fur);
  frame.fillEllipse(60, 177, 8, 5, fur);
  for (int16_t sx = -1; sx <= 1; sx += 2) {
    frame.fillTriangle(48 + sx * 24, hy + 6, 48 + sx * 24, hy - 24,
                       48 + sx * 8, hy - 14, fur);
    frame.fillTriangle(48 + sx * 20, hy + 1, 48 + sx * 21, hy - 17,
                       48 + sx * 11, hy - 10, pink);
  }
  frame.fillEllipse(48, hy, 25, 22, fur);
  frame.fillEllipse(48, hy + 8, 11, 8, belly);
  frame.fillTriangle(45, hy + 3, 51, hy + 3, 48, hy + 7, rgb(240, 110, 130));
  for (int16_t sx = -1; sx <= 1; sx += 2) {
    frame.drawLine(48 + sx * 12, hy + 6, 48 + sx * 30, hy + 1, whisker);
    frame.drawLine(48 + sx * 12, hy + 8, 48 + sx * 30, hy + 11, whisker);
  }
  for (int16_t sx = -10; sx <= 10; sx += 20) {
    if (blinking) {
      frame.drawFastHLine(48 + sx - 4, hy - 2, 9, rgb(90, 220, 90));
    } else {
      frame.fillEllipse(48 + sx, hy - 2, 4, 5, rgb(90, 220, 90));
      frame.fillEllipse(48 + sx, hy - 2, 1, 4, TFT_BLACK);
    }
  }
  frame.fillEllipse(40, hy - 16, 20, 8, rgb(150, 30, 50));
  frame.fillCircle(38, hy - 24, 2, rgb(150, 30, 50));
  thickLine(frame, 58, hy + 28, 58, hy + 66, 4, gold);
  drawThickArc(frame, 66, hy + 66, 8, 8, 90, 270, 4, gold);
  thickLine(frame, 66, hy + 74, 76, hy + 74, 4, gold);
  thickLine(frame, 76, hy + 74, 80, hy + 56, 4, gold);
  fillQuad(frame, 74, hy + 54, 86, hy + 54, 82, hy + 62, 80, hy + 58, gold);
  thickLine(frame, 56, hy + 22, 54, hy + 14, 2, gold);
  thickLine(frame, 36, 132 + b, 50, 124 + b + tap, 5, fur);
}

// Blitz the country horse with cowboy hat and neckerchief.
void drawBlitz(TFT_eSPI &frame, int16_t b, uint8_t pose, bool blinking) {
  constexpr int16_t O = -5;
  const uint16_t coat = rgb(150, 95, 55);
  const uint16_t dark = rgb(95, 55, 30);
  const uint16_t light = rgb(190, 135, 85);
  const uint16_t red = rgb(210, 50, 50);
  const uint16_t hatColour = rgb(120, 75, 40);
  const int16_t lift = pose % 6 < 3 ? 6 : 0;

  thickLine(frame, 18 + O + kWave[pose] / 2, 124 + b, 8 + O, 156, 4, dark);
  thickLine(frame, 20 + O, 128 + b, 12 + O, 160, 3, dark);
  for (int16_t lx = 24; lx <= 42; lx += 18) {
    thickLine(frame, lx + O, 140 + b, lx - 1 + O, 176, 6, dark);
    frame.fillRoundRect(lx - 5 + O, 175, 9, 6, 2, TFT_BLACK);
  }
  frame.fillEllipse(40 + O, 130 + b, 28, 18, coat);
  thickLine(frame, 30 + O, 140 + b, 31 + O, 176, 6, coat);
  frame.fillRoundRect(26 + O, 175, 9, 6, 2, TFT_BLACK);
  thickLine(frame, 62 + O, 140 + b, 63 + O + lift / 2, 176 - lift, 6, coat);
  frame.fillRoundRect(58 + O + lift / 2, 175 - lift, 9, 6, 2, TFT_BLACK);
  fillQuad(frame, 58 + O, 120 + b, 66 + O, 80 + b, 84 + O, 84 + b, 74 + O,
           130 + b, coat);
  frame.fillEllipse(84 + O, 90 + b, 9, 17, coat);
  frame.fillEllipse(91 + O, 102 + b, 5, 5, light);
  frame.fillCircle(92 + O, 102 + b, 1, TFT_BLACK);
  if (blinking) {
    frame.drawFastHLine(80 + O, 85 + b, 6, dark);
  } else {
    frame.fillCircle(83 + O, 85 + b, 2, TFT_BLACK);
  }
  for (int16_t i = 0; i < 5; ++i) {
    thickLine(frame, 62 + i * 2 + O, 80 + i * 8 + b, 56 + i * 2 + O,
              86 + i * 8 + b, 3, dark);
  }
  frame.fillTriangle(72 + O, 78 + b, 74 + O, 68 + b, 80 + O, 77 + b, coat);
  fillQuad(frame, 60 + O, 100 + b, 76 + O, 100 + b, 72 + O, 108 + b,
           64 + O, 108 + b, red);
  frame.fillTriangle(64 + O, 106 + b, 72 + O, 106 + b, 68 + O, 114 + b, red);
  frame.fillCircle(46 + O, 128 + b, 3, rgb(240, 190, 40));
  frame.fillEllipse(80 + O, 78 + b, 17, 4, hatColour);
  frame.fillRoundRect(72 + O, 64 + b, 16, 14, 4, hatColour);
  frame.fillRoundRect(72 + O, 73 + b, 16, 3, 1, red);
}

// Leopold the lion: singer with microphone, or king with crown and sceptre.
void drawLion(TFT_eSPI &frame, int16_t b, uint8_t pose, bool king,
              bool blinking) {
  const uint16_t coat = rgb(235, 185, 80);
  const uint16_t light = rgb(250, 225, 160);
  const uint16_t mane = rgb(215, 130, 30);
  const uint16_t maneDark = rgb(170, 95, 20);
  const uint16_t nose = rgb(60, 30, 30);
  const uint16_t gold = rgb(240, 190, 40);
  const int16_t tail = kWave[pose] / 3;

  thickLine(frame, 66, 168, 86, 176, 4, coat);
  thickLine(frame, 86, 176, 90, 160 + tail, 4, coat);
  frame.fillCircle(91, 157 + tail, 5, mane);
  frame.fillEllipse(48, 148 + b, 22, 30, coat);
  frame.fillEllipse(48, 154 + b, 13, 21, light);
  frame.fillEllipse(36, 176, 11, 6, coat);
  frame.fillEllipse(60, 176, 11, 6, coat);
  frame.fillCircle(48, 100 + b, 30, maneDark);
  frame.fillCircle(48, 100 + b, 26, mane);
  for (int16_t sx = -1; sx <= 1; sx += 2) {
    frame.fillCircle(48 + sx * 17, 76 + b, 6, coat);
    frame.fillCircle(48 + sx * 17, 76 + b, 3, rgb(240, 150, 150));
  }
  frame.fillCircle(48, 102 + b, 19, coat);
  frame.fillEllipse(48, 110 + b, 10, 7, light);
  frame.fillTriangle(45, 104 + b, 51, 104 + b, 48, 108 + b, nose);
  frame.drawLine(48, 108 + b, 48, 112 + b, nose);
  for (int16_t sx = -7; sx <= 7; sx += 14) {
    if (blinking) {
      frame.drawFastHLine(48 + sx - 3, 99 + b, 7, TFT_BLACK);
    } else {
      frame.fillCircle(48 + sx, 99 + b, 2, TFT_BLACK);
    }
  }

  if (!king) {
    const int16_t wave = kWave[pose] / 2;
    const uint16_t vest = rgb(25, 25, 30);
    const uint16_t stand = rgb(150, 150, 160);
    frame.fillEllipse(48, 114 + b, 4, pose % 4 < 2 ? 3 : 1, rgb(90, 40, 30));
    frame.fillRoundRect(28, 124 + b, 40, 24, 8, vest);
    for (int16_t sx = 34; sx <= 58; sx += 8) {
      frame.fillCircle(sx, 130 + b, 1, rgb(220, 220, 230));
    }
    thickLine(frame, 22, 176, 22, 118, 2, stand);
    thickLine(frame, 22, 118, 34, 118, 2, stand);
    frame.fillEllipse(36, 117, 5, 4, rgb(60, 60, 70));
    thickLine(frame, 33, 128 + b, 38, 118, 5, coat);
    thickLine(frame, 62, 128 + b, 78 + wave, 112 + b, 5, coat);
    frame.fillCircle(80 + wave, 109 + b, 3, coat);
    thickLine(frame, 80 + wave, 106 + b, 78 + wave, 98 + b, 2, coat);
    thickLine(frame, 82 + wave, 106 + b, 86 + wave, 99 + b, 2, coat);
  } else {
    frame.fillTriangle(33, 76 + b, 36, 60 + b, 42, 70 + b, gold);
    frame.fillTriangle(42, 70 + b, 48, 56 + b, 54, 70 + b, gold);
    frame.fillTriangle(54, 70 + b, 60, 60 + b, 63, 76 + b, gold);
    frame.fillRoundRect(33, 74 + b, 30, 4, 1, gold);
    frame.fillCircle(48, 71 + b, 2, pose % 6 < 3 ? TFT_RED : TFT_WHITE);
    frame.fillTriangle(26, 122 + b, 70, 122 + b, 82, 176, rgb(150, 30, 50));
    frame.fillTriangle(26, 122 + b, 82, 176, 14, 176, rgb(150, 30, 50));
    fillQuad(frame, 40, 122 + b, 56, 122 + b, 60, 150 + b, 36, 150 + b,
             rgb(245, 240, 235));
    for (int16_t sx = 40; sx <= 56; sx += 8) {
      frame.fillCircle(sx, 130 + b, 1, rgb(30, 30, 30));
    }
    thickLine(frame, 62, 130 + b, 82, 150 + b, 5, coat);
    thickLine(frame, 86, 118 + b, 86, 150 + b, 3, gold);
    frame.fillCircle(86, 114 + b, 5, gold);
    frame.fillCircle(86, 114 + b, 2, TFT_RED);
  }
}

// Ludwig the swan with an S-shaped neck: conductor or baroque violinist.
void drawSwan(TFT_eSPI &frame, int16_t b, uint8_t pose, bool violin,
              bool blinking) {
  const uint16_t white = rgb(245, 245, 250);
  const uint16_t shade = rgb(200, 205, 225);
  const uint16_t beak = rgb(250, 150, 30);
  const uint16_t suit = rgb(30, 30, 40);
  const int16_t ripple = kWave[pose] / 3;
  const int16_t headX = 70;
  const int16_t headY = 76 + b / 2;

  for (int16_t i = 0; i < 3; ++i) {
    thickLine(frame, 8 + i * 6 + ripple, 172 + i * 6, 86 - i * 8 + ripple,
              172 + i * 6, 2, rgb(70, 110, 190));
  }
  frame.fillTriangle(26, 152, 10, 132, 36, 142, white);
  frame.fillEllipse(46, 152, 28, 16, white);
  frame.fillEllipse(40, 154, 18, 9, shade);
  for (int16_t i = 0; i <= 40; ++i) {
    const float t = i / 40.0f;
    const float u = 1.0f - t;
    const float x = u * u * u * 60 + 3 * u * u * t * 88 +
                    3 * u * t * t * 40 + t * t * t * headX;
    const float y = u * u * u * 140 + 3 * u * u * t * 128 +
                    3 * u * t * t * 100 + t * t * t * headY;
    frame.fillCircle(lroundf(x), lroundf(y), lroundf(4.0f - 1.5f * t),
                     white);
  }
  frame.fillCircle(headX, headY, 8, white);
  frame.fillTriangle(headX + 6, headY - 3, headX + 22, headY + 2, headX + 6,
                     headY + 7, beak);
  frame.drawLine(headX + 6, headY - 3, headX + 8, headY, TFT_BLACK);
  if (blinking) {
    frame.drawFastHLine(headX - 2, headY - 2, 7, rgb(100, 115, 135));
  } else {
    frame.fillCircle(headX + 1, headY - 2, 2, TFT_BLACK);
  }

  if (!violin) {
    frame.fillEllipse(headX, headY - 9, 12, 3, suit);
    frame.fillRoundRect(headX - 7, headY - 21, 14, 12, 3, suit);
    frame.fillRoundRect(headX - 7, headY - 14, 14, 3, 1, TFT_RED);
    fillQuad(frame, 46, 138, 52, 134, 58, 138, 52, 142, rgb(200, 40, 50));
    thickLine(frame, 36, 146, 60, 150, 6, suit);
    thickLine(frame, 50, 144, 76, 124, 5, white);
    thickLine(frame, 76, 124, 88 + kWave[pose], 100, 2, rgb(240, 230, 200));
    frame.fillCircle(76, 124, 3, white);
  } else {
    const uint16_t wig = rgb(232, 232, 238);
    const int16_t bow = kWave[pose];
    frame.fillEllipse(headX, headY - 9, 13, 6, wig);
    for (int16_t sx = -10; sx <= 10; sx += 20) {
      frame.fillCircle(headX + sx, headY, 4, wig);
      frame.fillCircle(headX + sx, headY + 7, 4, wig);
    }
    frame.fillRoundRect(14, 132, 34, 22, 8, rgb(180, 100, 40));
    frame.fillCircle(31, 143, 4, rgb(60, 30, 10));
    thickLine(frame, 48, 138, 74, 118, 3, rgb(110, 60, 25));
    thickLine(frame, 8 + bow, 120 + bow / 2, 64 + bow, 152 + bow / 2, 2,
              rgb(190, 180, 160));
    thickLine(frame, 56, 146, 30, 138, 5, white);
  }
}

// Poldi the peacock: full fan, or rock star with hat, bow tie and microphone.
void drawPeacock(TFT_eSPI &frame, int16_t b, uint8_t pose, bool rock,
                 bool blinking) {
  const uint16_t blue = rgb(20, 160, 200);
  const uint16_t darkBlue = rgb(10, 110, 160);
  const uint16_t green = rgb(40, 200, 110);
  const uint16_t gold = rgb(240, 190, 40);
  const int count = rock ? 7 : 9;
  const float open = (pose <= 6 ? pose : 12 - pose) / 6.0f;
  const float spread =
      (rock ? 30.0f : 40.0f) + open * (rock ? 45.0f : 65.0f);
  const float length = (rock ? 56.0f : 50.0f) + (pose % 6 < 3 ? 0 : 3);

  for (int i = 0; i < count; ++i) {
    const float angle =
        (-90.0f - spread / 2 + spread * i / (count - 1)) * 0.0174533f;
    const int16_t fx = 48 + lroundf(length * cosf(angle));
    const int16_t fy = 150 + b + lroundf(length * sinf(angle));
    thickLine(frame, 48, 150 + b, fx, fy, 2, darkBlue);
    frame.fillEllipse(fx, fy, 6, 8, green);
    frame.fillEllipse(fx, fy, 4, 6, blue);
    frame.fillCircle(fx, fy, 2, (i + pose / 3) % 3 == 0 ? TFT_WHITE : gold);
  }
  thickLine(frame, 42, 170, 40, 184, 2, gold);
  thickLine(frame, 54, 170, 56, 184, 2, gold);
  frame.fillEllipse(48, 150 + b, 14, 24, blue);
  frame.fillEllipse(48, 154 + b, 8, 17, rgb(170, 235, 245));
  thickLine(frame, 50, 128 + b, 52, 112 + b, 6, blue);
  frame.fillCircle(52, 106 + b, 9, blue);
  frame.fillTriangle(58, 104 + b, 68, 109 + b, 58, 112 + b, gold);
  if (blinking && !rock) {
    frame.drawFastHLine(52, 103 + b, 7, darkBlue);
  } else if (!rock) {
    frame.fillCircle(55, 103 + b, 2, TFT_BLACK);
  }
  for (int16_t dx = -3; dx <= 3; dx += 3) {
    thickLine(frame, 52 + dx, 98 + b, 52 + dx * 2, 88 + b, 1, darkBlue);
    frame.fillCircle(52 + dx * 2, 87 + b, 2, green);
  }
  if (rock) {
    const int16_t wave = kWave[pose] / 3;
    frame.fillRoundRect(44, 92 + b, 18, 7, 2, TFT_BLACK);
    frame.drawLine(62, 95 + b, 66, 96 + b, TFT_BLACK);
    fillQuad(frame, 44, 128 + b, 56, 128 + b, 50, 136 + b, 50, 136 + b,
             rgb(230, 40, 130));
    frame.fillTriangle(50, 128 + b, 44, 124 + b, 44, 132 + b, rgb(230, 40, 130));
    thickLine(frame, 36, 140 + b, 22, 148 + b, 4, blue);
    thickLine(frame, 22, 148 + b, 18 + wave, 120 + b, 2, rgb(90, 90, 100));
    frame.fillCircle(18 + wave, 116 + b, 5, rgb(110, 110, 120));
  }
}

// Hansi the budgie on a perch: glitter jacket with microphone, or folk outfit.
void drawBudgie(TFT_eSPI &frame, int16_t b, uint8_t pose, bool glitter,
                bool blinking) {
  const uint16_t blue = rgb(30, 180, 230);
  const uint16_t darkBlue = rgb(10, 110, 160);
  const uint16_t yellow = rgb(250, 230, 90);
  const uint16_t orange = rgb(240, 150, 40);
  const uint16_t gold = rgb(240, 190, 40);
  const int16_t wave = kWave[pose] / 3;

  thickLine(frame, 10, 177, 86, 177, 3, rgb(120, 80, 40));
  thickLine(frame, 44, 170, 44, 176, 2, orange);
  thickLine(frame, 56, 170, 56, 176, 2, orange);
  frame.fillTriangle(36, 150 + b, 24, 188, 50, 168 + b, darkBlue);
  frame.fillEllipse(50, 140 + b, 20, 30, blue);
  frame.fillEllipse(52, 150 + b, 12, 20, rgb(120, 215, 240));
  frame.fillEllipse(40, 138 + b, 8, 20, rgb(20, 130, 190));
  frame.fillEllipse(52, 92 + b, 17, 17, yellow);
  frame.fillEllipse(54, 102 + b, 13, 8, yellow);
  frame.fillTriangle(62, 92 + b, 76, 96 + b, 62, 102 + b, orange);
  frame.fillCircle(56, 88 + b, 4, TFT_WHITE);
  if (blinking) {
    frame.drawFastHLine(52, 88 + b, 9, yellow);
  } else {
    frame.fillCircle(57, 88 + b, 2, TFT_BLACK);
  }
  for (int16_t dx = 0; dx <= 12; dx += 6) {
    frame.fillCircle(40 + dx, 104 + b, 1, rgb(20, 20, 60));
  }
  frame.fillTriangle(38, 82 + b, 42, 70 + b, 48, 80 + b, blue);

  if (glitter) {
    frame.fillRoundRect(34, 118 + b, 32, 40, 8, rgb(150, 30, 160));
    frame.fillCircle(40, 126 + b, 1, gold);
    frame.fillCircle(54, 134 + b, 1, rgb(255, 120, 220));
    frame.fillCircle(44, 146 + b, 1, rgb(120, 240, 255));
    frame.fillCircle(58, 152 + b, 1, gold);
    frame.fillTriangle(46, 118 + b, 50, 128 + b, 54, 118 + b, TFT_WHITE);
    thickLine(frame, 62, 124 + b, 78, 110 + b + wave, 5, blue);
    thickLine(frame, 80, 112 + b + wave, 83, 98 + b + wave, 2,
              rgb(130, 130, 140));
    frame.fillCircle(84, 95 + b + wave, 4, rgb(110, 110, 120));
    thickLine(frame, 38, 126 + b, 26, 136 + b, 5, blue);
    if (pose % 4 < 2) {
      frame.drawLine(88, 88 + b, 94, 84 + b, rgb(255, 255, 120));
      frame.drawLine(89, 92 + b, 95, 92 + b, rgb(255, 255, 120));
    }
  } else {
    const uint16_t hat = rgb(110, 80, 50);
    const uint16_t green = rgb(60, 120, 60);
    frame.fillEllipse(52, 76 + b, 14, 5, hat);
    frame.fillRoundRect(42, 62 + b, 20, 15, 4, hat);
    frame.fillRoundRect(42, 70 + b, 20, 3, 1, rgb(210, 40, 40));
    frame.drawLine(60, 64 + b, 66, 52 + b, rgb(60, 160, 60));
    frame.drawLine(62, 64 + b, 70, 56 + b, rgb(220, 60, 60));
    frame.fillRoundRect(32, 124 + b, 36, 32, 6, green);
    frame.fillRoundRect(30, 120 + b, 40, 7, 3, rgb(130, 80, 40));
    frame.drawLine(40, 124 + b, 40, 138 + b, rgb(200, 200, 200));
    frame.drawLine(60, 124 + b, 60, 138 + b, rgb(200, 200, 200));
    thickLine(frame, 38, 130 + b, 20, 138 + b, 5, blue);
    thickLine(frame, 62, 130 + b, 82, 118 + b + wave, 5, blue);
    frame.fillCircle(84, 114 + b + wave, 3, blue);
    frame.fillEllipse(20, 134 + b, 5, 8, rgb(230, 200, 120));
  }
}

// Dispatch to the selected animal renderer and add dance-specific accents.
void drawAnimal(TFT_eSPI &frame, uint8_t index, int16_t bob,
                PetDance dance, uint8_t pose, bool blinking) {
  static const int8_t sway[] = {0, 1, 2, 3, 2, 1, 0, -1, -2, -3, -2, -1};
  const int16_t x =
      48 + (dance == PetDance::Sway ? sway[pose] : dance == PetDance::Hop
                                                        ? sway[(pose + 3) % 12]
                                                        : 0);
  const int16_t y = 137 + bob;
  const uint16_t fur = 0xAD55;
  const uint16_t shadow = 0x6B4D;

  switch (index) {
    case PeacockPet: {
      drawPeacock(frame, bob, pose, dance == PetDance::Sway, blinking);
      break;
    }
    case LionPet: {
      drawLion(frame, bob, pose, dance == PetDance::Sway, blinking);
      break;
    }
    case KangarooPet: {
      drawKangaroo(frame, bob, pose, dance == PetDance::Sway, blinking);
      break;
    }
    case ChameleonPet: {
      drawChameleon(frame, bob, pose, dance == PetDance::Sway, blinking);
      break;
    }
    case SwanPet: {
      drawSwan(frame, bob, pose, dance == PetDance::Sway, blinking);
      break;
    }
    case MoritzPet: {
      drawMoritz(frame, bob, pose, blinking);
      break;
    }
    case RhinoPet: {
      drawRhino(frame, bob, pose, dance == PetDance::Sway, blinking);
      break;
    }
    case WilliPet: {
      if (dance == PetDance::Sway) {
        drawWilliHanging(frame, sway[pose] * 2, bob, pose, blinking);
      } else {
        drawWilliSitting(frame, bob, pose, blinking);
      }
      break;
    }
    case BlitzPet: {
      drawBlitz(frame, bob, pose, blinking);
      break;
    }
    case HansiPet: {
      drawBudgie(frame, bob, pose, dance == PetDance::Sway, blinking);
      break;
    }
  }

  if (dance == PetDance::Sway) {
    drawPetMusicNote(frame, 13 + sway[pose], 91, TFT_CYAN);
    drawPetMusicNote(frame, 79 - sway[pose], 112, TFT_YELLOW);
  } else if (dance == PetDance::Hop && pose % 3 == 0) {
    drawPetSparkle(frame, 16, 166, TFT_YELLOW);
    drawPetSparkle(frame, 84, 159, TFT_CYAN);
  } else if (pose % 6 == 0) {
    frame.fillCircle(14, 90, 2, TFT_YELLOW);
    frame.fillCircle(82, 108, 2, TFT_CYAN);
  }
}

}  // namespace

// Compose the selected pet frame from its background, pose, outfit, and label.
void drawPet(TFT_eSPI &frameBuffer, uint32_t now, const Pet &pet,
             PetDance dance, PetOutfit outfit) {
  if (pet.drawingIndex < 0) {
    return;
  }

  drawFrame(frameBuffer);
  const uint8_t pose = (now / FAT_CAT_FRAME_INTERVAL_MS) % 12;
  static const int8_t bounce[] = {0, -2, -5, -8, -10, -8,
                                  -5, -2, 0, 2, 1, 0};
  static const int8_t sway[] = {0, -1, -2, -3, -2, -1, 0, 1, 2, 3, 2, 1};
  static const int8_t hops[] = {0, -4, -8, -12, -8, -4,
                                0, -3, -7, -11, -7, -3};
  const int16_t bob = dance == PetDance::Sway
                          ? sway[pose]
                          : dance == PetDance::Hop ? hops[pose] : bounce[pose];
  const uint8_t index = static_cast<uint8_t>(pet.drawingIndex);
  const uint8_t petIndex = index + 1;
  drawAnimal(frameBuffer, index, bob, dance, pose,
             isPetBlinking(now, petIndex));
  drawPetClothing(frameBuffer, index, outfit, bob);
}
