#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <esp_heap_caps.h>

#define TFT_BLACK RGB565_BLACK
#define TFT_WHITE RGB565_WHITE
#define TFT_RED RGB565_RED
#define TFT_CYAN RGB565_CYAN
#define TFT_YELLOW RGB565_YELLOW

constexpr uint8_t TL_DATUM = 0;
constexpr uint8_t TR_DATUM = 2;
constexpr uint8_t MC_DATUM = 4;

namespace jc3248w535 {

constexpr int16_t APP_WIDTH = 320;
constexpr int16_t APP_HEIGHT = 240;
constexpr int16_t DISPLAY_WIDTH = 480;
constexpr int16_t DISPLAY_HEIGHT = 320;
constexpr int16_t PANEL_WIDTH = 320;
constexpr int16_t PANEL_HEIGHT = 480;
constexpr size_t OUTPUT_BUFFER_BYTES =
    PANEL_WIDTH * PANEL_HEIGHT * sizeof(uint16_t);
constexpr size_t FRAMEBUFFER_BYTES =
    DISPLAY_WIDTH * DISPLAY_HEIGHT * sizeof(uint16_t);

extern Arduino_ESP32QSPI displayBus;
extern Arduino_AXS15231B displayPanel;

inline int16_t scaleX(int32_t x) {
  return static_cast<int16_t>(x * DISPLAY_WIDTH / APP_WIDTH);
}

inline int16_t scaleY(int32_t y) {
  return static_cast<int16_t>(y * DISPLAY_HEIGHT / APP_HEIGHT);
}

}  // namespace jc3248w535

class TFT_eSPI : public Arduino_Canvas {
 public:
  TFT_eSPI()
      : Arduino_Canvas(jc3248w535::DISPLAY_WIDTH,
                       jc3248w535::DISPLAY_HEIGHT,
                       &jc3248w535::displayPanel) {}

  bool init() {
    pinMode(1, OUTPUT);
    digitalWrite(1, HIGH);
    if (!jc3248w535::displayPanel.begin()) {
      Serial.println("ERROR: could not initialize the AXS15231B display.");
      return false;
    }

    _framebuffer = static_cast<uint16_t *>(
        heap_caps_malloc(jc3248w535::FRAMEBUFFER_BYTES,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (_framebuffer == nullptr) {
      Serial.printf("ERROR: could not allocate %u-byte display frame buffer "
                    "in PSRAM.\n",
                    static_cast<unsigned>(jc3248w535::FRAMEBUFFER_BYTES));
      return false;
    }

    outputBuffer_ = static_cast<uint16_t *>(
        heap_caps_malloc(jc3248w535::OUTPUT_BUFFER_BYTES,
                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (outputBuffer_ == nullptr) {
      free(_framebuffer);
      _framebuffer = nullptr;
      Serial.printf("ERROR: could not allocate %u-byte display output buffer "
                    "in PSRAM.\n",
                    static_cast<unsigned>(jc3248w535::OUTPUT_BUFFER_BYTES));
      return false;
    }

    jc3248w535::displayPanel.setRotation(0);
    Arduino_Canvas::setRotation(0);
    markDirty(0, 0, jc3248w535::APP_WIDTH, jc3248w535::APP_HEIGHT);
    return true;
  }

  void setRotation(uint8_t) {
    jc3248w535::displayPanel.setRotation(0);
    Arduino_Canvas::setRotation(0);
    markDirty(0, 0, jc3248w535::APP_WIDTH, jc3248w535::APP_HEIGHT);
  }

  void setTextDatum(uint8_t datum) { textDatum_ = datum; }

  void setTextSize(uint8_t size) { textSize_ = max<uint8_t>(size, 1); }

  void setNativeCoordinates(bool enabled) {
    nativeCoordinates_ = enabled;
    resetViewport();
  }

  int16_t textWidth(const String &text, uint8_t font = 1) const {
    return text.length() * 6 * effectiveTextSize(font);
  }

  int16_t textWidth(const char *text, uint8_t font = 1) const {
    return textWidth(String(text), font);
  }

  int16_t drawString(const String &text, int32_t x, int32_t y,
                     uint8_t font = 1) {
    const uint8_t size = effectiveTextSize(font);
    const int16_t width = textWidth(text, font);
    const int16_t height = 8 * size;
    if (textDatum_ == TR_DATUM) {
      x -= width;
    } else if (textDatum_ == MC_DATUM) {
      x -= width / 2;
      y -= height / 2;
    }
    Arduino_Canvas::setTextSize(size);
    Arduino_Canvas::setCursor(x, y);
    Arduino_Canvas::print(text);
    return width;
  }

  int16_t drawCentreString(const String &text, int32_t x, int32_t y,
                           uint8_t font = 1) {
    const uint8_t previousDatum = textDatum_;
    textDatum_ = MC_DATUM;
    const int16_t width = drawString(text, x, y, font);
    textDatum_ = previousDatum;
    return width;
  }

  uint16_t color565(uint8_t red, uint8_t green, uint8_t blue) const {
    return RGB565(red, green, blue);
  }

  void setViewport(int32_t x, int32_t y, int32_t width, int32_t height,
                   bool = false) {
    clipLeft_ = max<int32_t>(0, x);
    clipTop_ = max<int32_t>(0, y);
    clipRight_ = min<int32_t>(coordinateWidth(), x + width);
    clipBottom_ = min<int32_t>(coordinateHeight(), y + height);
  }

  void resetViewport() {
    clipLeft_ = 0;
    clipTop_ = 0;
    clipRight_ = coordinateWidth();
    clipBottom_ = coordinateHeight();
  }

  void writePixelPreclipped(int16_t x, int16_t y,
                            uint16_t color) override {
    if (!insideViewport(x, y)) {
      return;
    }
    const int16_t left = transformX(x);
    const int16_t top = transformY(y);
    const int16_t right = transformX(x + 1);
    const int16_t bottom = transformY(y + 1);
    markDirty(x, y, x + 1, y + 1);
    Arduino_Canvas::writeFillRectPreclipped(left, top, right - left,
                                            bottom - top, color);
  }

  void writeFastHLine(int16_t x, int16_t y, int16_t width,
                      uint16_t color) override {
    if (y < clipTop_ || y >= clipBottom_ || width <= 0) {
      return;
    }
    const int32_t left = max<int32_t>(x, clipLeft_);
    const int32_t right = min<int32_t>(x + width, clipRight_);
    if (right <= left) {
      return;
    }
    const int16_t nativeLeft = transformX(left);
    const int16_t nativeTop = transformY(y);
    markDirty(left, y, right, y + 1);
    Arduino_Canvas::writeFillRectPreclipped(
        nativeLeft, nativeTop, transformX(right) - nativeLeft,
        transformY(y + 1) - nativeTop, color);
  }

  void writeFastVLine(int16_t x, int16_t y, int16_t height,
                      uint16_t color) override {
    if (x < clipLeft_ || x >= clipRight_ || height <= 0) {
      return;
    }
    const int32_t top = max<int32_t>(y, clipTop_);
    const int32_t bottom = min<int32_t>(y + height, clipBottom_);
    if (bottom <= top) {
      return;
    }
    const int16_t nativeLeft = transformX(x);
    const int16_t nativeTop = transformY(top);
    markDirty(x, top, x + 1, bottom);
    Arduino_Canvas::writeFillRectPreclipped(
        nativeLeft, nativeTop, transformX(x + 1) - nativeLeft,
        transformY(bottom) - nativeTop, color);
  }

  void writeFillRectPreclipped(int16_t x, int16_t y, int16_t width,
                               int16_t height, uint16_t color) override {
    if (width <= 0 || height <= 0) {
      return;
    }
    const int32_t left = max<int32_t>(x, clipLeft_);
    const int32_t top = max<int32_t>(y, clipTop_);
    const int32_t right =
        min<int32_t>(x + width, min<int32_t>(clipRight_, coordinateWidth()));
    const int32_t bottom =
        min<int32_t>(y + height,
                     min<int32_t>(clipBottom_, coordinateHeight()));
    if (right <= left || bottom <= top) {
      return;
    }
    const int16_t nativeLeft = transformX(left);
    const int16_t nativeTop = transformY(top);
    markDirty(left, top, right, bottom);
    Arduino_Canvas::writeFillRectPreclipped(
        nativeLeft, nativeTop, transformX(right) - nativeLeft,
        transformY(bottom) - nativeTop, color);
  }

  bool flushFrame(void (*serviceAudio)() = nullptr) {
    if (!dirty_) {
      return true;
    }
    if (outputBuffer_ == nullptr || getFramebuffer() == nullptr) {
      Serial.println("ERROR: display frame buffer is unavailable.");
      return false;
    }

    constexpr int16_t panelWidth = jc3248w535::PANEL_WIDTH;
    constexpr int16_t panelHeight = jc3248w535::PANEL_HEIGHT;
    const uint16_t *source = getFramebuffer();

    for (int16_t y = 0; y < panelHeight; ++y) {
      const int16_t sourceX = jc3248w535::DISPLAY_WIDTH - 1 - y;
      uint16_t *outputRow = outputBuffer_ + y * panelWidth;
      for (int16_t x = 0; x < panelWidth; ++x) {
        const int16_t sourceY = x;
        outputRow[x] =
            source[sourceY * jc3248w535::DISPLAY_WIDTH + sourceX];
      }
      if (serviceAudio != nullptr && (y & 0x0F) == 0) {
        serviceAudio();
      }
    }

    if (serviceAudio != nullptr) {
      serviceAudio();
    }
    jc3248w535::displayPanel.draw16bitRGBBitmap(
        0, 0, outputBuffer_, panelWidth, panelHeight);
    if (serviceAudio != nullptr) {
      serviceAudio();
    }
    dirty_ = false;
    dirtyLeft_ = jc3248w535::APP_WIDTH;
    dirtyTop_ = jc3248w535::APP_HEIGHT;
    dirtyRight_ = 0;
    dirtyBottom_ = 0;
    return true;
  }

 private:
  int32_t coordinateWidth() const {
    return nativeCoordinates_ ? jc3248w535::DISPLAY_WIDTH
                              : jc3248w535::APP_WIDTH;
  }

  int32_t coordinateHeight() const {
    return nativeCoordinates_ ? jc3248w535::DISPLAY_HEIGHT
                              : jc3248w535::APP_HEIGHT;
  }

  int16_t transformX(int32_t x) const {
    return nativeCoordinates_ ? static_cast<int16_t>(x)
                              : jc3248w535::scaleX(x);
  }

  int16_t transformY(int32_t y) const {
    return nativeCoordinates_ ? static_cast<int16_t>(y)
                              : jc3248w535::scaleY(y);
  }

  uint8_t effectiveTextSize(uint8_t font) const {
#if defined(BOARD_JC3248W535)
    if (nativeCoordinates_) {
      return max<uint8_t>(font * textSize_, 1);
    }
    return max<uint8_t>(
        static_cast<uint8_t>(font * textSize_ / 2), 1);
#else
    return max<uint8_t>(font * textSize_, 1);
#endif
  }

  bool insideViewport(int16_t x, int16_t y) const {
    return x >= clipLeft_ && x < clipRight_ && y >= clipTop_ &&
           y < clipBottom_;
  }

  void markDirty(int32_t left, int32_t top, int32_t right, int32_t bottom) {
    dirtyLeft_ = min(dirtyLeft_, left);
    dirtyTop_ = min(dirtyTop_, top);
    dirtyRight_ = max(dirtyRight_, right);
    dirtyBottom_ = max(dirtyBottom_, bottom);
    dirty_ = true;
  }

  uint16_t *outputBuffer_ = nullptr;
  uint8_t textDatum_ = TL_DATUM;
  uint8_t textSize_ = 1;
  int32_t clipLeft_ = 0;
  int32_t clipTop_ = 0;
  int32_t clipRight_ = jc3248w535::APP_WIDTH;
  int32_t clipBottom_ = jc3248w535::APP_HEIGHT;
  int32_t dirtyLeft_ = jc3248w535::APP_WIDTH;
  int32_t dirtyTop_ = jc3248w535::APP_HEIGHT;
  int32_t dirtyRight_ = 0;
  int32_t dirtyBottom_ = 0;
  bool dirty_ = false;
  bool nativeCoordinates_ = false;
};

class TFT_eSprite : public TFT_eSPI {
 public:
  explicit TFT_eSprite(TFT_eSPI *) {}

  void setColorDepth(uint8_t) {}
  bool createSprite(int16_t, int16_t) { return false; }
  void fillSprite(uint32_t) {}
  void *getPointer() { return nullptr; }
  void pushSprite(int32_t, int32_t) {}
  void pushSprite(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t) {}
};
