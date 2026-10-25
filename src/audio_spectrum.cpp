#include <math.h>
#include <cstring>

#include "audio_compressor.h"
#include "audio_dac.h"
#include "audio_spectrum.h"
#include "fat_cat.h"

namespace {

constexpr uint16_t FFT_SIZE = 256;
constexpr uint8_t AUDIO_BLOCK_COUNT = 3;
constexpr uint8_t SPECTRUM_BANDS = 8;
#if defined(BOARD_JC3248W535)
constexpr uint32_t SPECTRUM_REFRESH_INTERVAL_MS = 67;
constexpr uint32_t AUDIO_STALE_MS = 1200;
constexpr uint8_t SPECTRUM_ATTACK_STEP = 8;
constexpr uint8_t SPECTRUM_RELEASE_STEP = 2;
constexpr int16_t SPECTRUM_BASELINE = 312;
constexpr int16_t SPECTRUM_TOP = 265;
constexpr int16_t SPECTRUM_HEIGHT = 55;
constexpr int16_t SPECTRUM_MAX_HEIGHT = 44;
#else
constexpr uint32_t SPECTRUM_REFRESH_INTERVAL_MS = 250;
constexpr uint32_t AUDIO_STALE_MS = 300;
constexpr uint8_t SPECTRUM_ATTACK_STEP = 3;
constexpr uint8_t SPECTRUM_RELEASE_STEP = 3;
constexpr int16_t SPECTRUM_BASELINE = 238;
constexpr int16_t SPECTRUM_TOP = 195;
constexpr int16_t SPECTRUM_HEIGHT = 45;
constexpr int16_t SPECTRUM_MAX_HEIGHT = 32;
#endif
constexpr float TILT_DB_PER_OCTAVE = 6.0f;
constexpr float MIN_ANALYZABLE_MAGNITUDE = 0.0001f;

struct AudioBlock {
  int16_t samples[FFT_SIZE];
};

constexpr uint16_t BAND_EDGES[SPECTRUM_BANDS + 1] = {
    1, 2, 4, 7, 12, 20, 34, 58, 128};
constexpr uint16_t BAND_COLOURS[SPECTRUM_BANDS] = {
    0xF800, 0xFBE0, 0xFFE0, 0x87E0, 0x07E0, 0x07FF, 0x051F, 0xA81F};

AudioBlock audioBlocks[AUDIO_BLOCK_COUNT]{};
volatile uint8_t queuedBlocks = 0;
volatile bool captureEnabled = true;
uint8_t writeBlock = 0;
uint16_t captureIndex = 0;
volatile uint32_t lastAudioBlockAt = 0;
portMUX_TYPE audioBlocksMux = portMUX_INITIALIZER_UNLOCKED;
uint32_t lastSpectrumAnalysisAt = 0;
float real[FFT_SIZE]{};
float imaginary[FFT_SIZE]{};
float referenceDecibels = -60.0f;
uint8_t targetHeights[SPECTRUM_BANDS]{};
uint8_t displayedHeights[SPECTRUM_BANDS]{};

// Transform one captured audio block and convert its bins into smoothed band
// targets, compensating for the typical treble roll-off in music.
void analyzeBlock(const AudioBlock &block) {
  constexpr float FFT_TWO_PI = 6.28318530718f;
  for (uint16_t i = 0; i < FFT_SIZE; ++i) {
    const float window =
        0.5f -
        0.5f * cosf(FFT_TWO_PI * i / static_cast<float>(FFT_SIZE - 1));
    real[i] = block.samples[i] * window;
    imaginary[i] = 0.0f;
  }

  for (uint16_t i = 1, j = 0; i < FFT_SIZE; ++i) {
    uint16_t bit = FFT_SIZE >> 1;
    for (; j & bit; bit >>= 1) {
      j ^= bit;
    }
    j ^= bit;
    if (i < j) {
      const float realValue = real[i];
      real[i] = real[j];
      real[j] = realValue;
    }
  }

  for (uint16_t length = 2; length <= FFT_SIZE; length <<= 1) {
    const float angle = -FFT_TWO_PI / length;
    const float stepReal = cosf(angle);
    const float stepImaginary = sinf(angle);
    for (uint16_t start = 0; start < FFT_SIZE; start += length) {
      float twiddleReal = 1.0f;
      float twiddleImaginary = 0.0f;
      const uint16_t halfLength = length >> 1;
      for (uint16_t offset = 0; offset < halfLength; ++offset) {
        const uint16_t even = start + offset;
        const uint16_t odd = even + halfLength;
        const float oddReal =
            real[odd] * twiddleReal - imaginary[odd] * twiddleImaginary;
        const float oddImaginary =
            real[odd] * twiddleImaginary + imaginary[odd] * twiddleReal;
        const float evenReal = real[even];
        const float evenImaginary = imaginary[even];

        real[even] = evenReal + oddReal;
        imaginary[even] = evenImaginary + oddImaginary;
        real[odd] = evenReal - oddReal;
        imaginary[odd] = evenImaginary - oddImaginary;

        const float nextTwiddleReal =
            twiddleReal * stepReal - twiddleImaginary * stepImaginary;
        twiddleImaginary =
            twiddleReal * stepImaginary + twiddleImaginary * stepReal;
        twiddleReal = nextTwiddleReal;
      }
    }
  }

  constexpr float NORMALIZATION = FFT_SIZE * 0.25f * 32768.0f;
  float bandDecibels[SPECTRUM_BANDS];
  float loudestDecibels = -60.0f;
  for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
    float bandPower = 0.0f;
    const uint16_t firstBin = BAND_EDGES[band];
    const uint16_t endBin = BAND_EDGES[band + 1];
    for (uint16_t bin = firstBin; bin < endBin; ++bin) {
      const float magnitude =
          sqrtf(real[bin] * real[bin] +
                imaginary[bin] * imaginary[bin]) /
          NORMALIZATION;
      bandPower += magnitude * magnitude;
    }

    const float magnitude =
        sqrtf(bandPower / static_cast<float>(endBin - firstBin));
    // Compensate the natural treble roll-off of music (about 4.5 dB/octave).
    const float centreBin =
        sqrtf(static_cast<float>(firstBin) * static_cast<float>(endBin));
    const float decibels =
        20.0f * log10f(max(magnitude, MIN_ANALYZABLE_MAGNITUDE)) +
        TILT_DB_PER_OCTAVE * log2f(centreBin / BAND_EDGES[0]);
    bandDecibels[band] = decibels;
    loudestDecibels = max(loudestDecibels, decibels);
  }

  // Auto gain: the loudest band sets the reference, which falls slowly so
  // quiet passages and low volume settings still fill the bars.
  referenceDecibels = loudestDecibels > referenceDecibels
                          ? loudestDecibels
                          : max(loudestDecibels, referenceDecibels - 1.0f);
  const float topDecibels = max(referenceDecibels, -60.0f) + 3.0f;
  for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
    const float scaledHeight = (bandDecibels[band] - (topDecibels - 36.0f)) *
                               SPECTRUM_MAX_HEIGHT / 36.0f;
    targetHeights[band] = scaledHeight > 0.0f
                              ? min<int16_t>(SPECTRUM_MAX_HEIGHT,
                                             static_cast<int16_t>(scaledHeight))
                              : 0;
  }
}

}  // namespace

// Enable or disable sample capture and reset pending blocks and bar history.
void setAudioSpectrumEnabled(bool enabled) {
  portENTER_CRITICAL(&audioBlocksMux);
  captureEnabled = enabled;
  queuedBlocks = 0;
  writeBlock = 0;
  captureIndex = 0;
  lastAudioBlockAt = 0;
  portEXIT_CRITICAL(&audioBlocksMux);
  lastSpectrumAnalysisAt = 0;
  referenceDecibels = -60.0f;
  for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
    targetHeights[band] = 0;
    displayedHeights[band] = 0;
  }
}

namespace {

// Store mono samples in the bounded ring of blocks consumed by the UI task.
void captureSpectrumSample(uint32_t sample) {
  const int16_t left = static_cast<int16_t>(sample >> 16);
  const int16_t right = static_cast<int16_t>(sample & 0xFFFF);
  portENTER_CRITICAL(&audioBlocksMux);
  if (captureEnabled) {
    audioBlocks[writeBlock].samples[captureIndex++] =
        static_cast<int16_t>((static_cast<int32_t>(left) + right) / 2);

    if (captureIndex == FFT_SIZE) {
      captureIndex = 0;
      lastAudioBlockAt = millis();
      writeBlock = (writeBlock + 1) % AUDIO_BLOCK_COUNT;
      // Keep the newest window rolling when analysis falls behind.
      if (queuedBlocks < AUDIO_BLOCK_COUNT) {
        ++queuedBlocks;
      }
    }
  }
  portEXIT_CRITICAL(&audioBlocksMux);
}

}  // namespace

// Chain DSP stages in the decoder's pre-I2S callback; continueI2S is retained
// for the audio library's callback contract.
void audio_process_i2s(uint32_t *sample, bool *continueI2S) {
  *continueI2S = true;
  processAudioCompressor(sample);
  captureSpectrumSample(*sample);
  processAudioDac(sample);
}

// Analyze pending audio blocks and draw the spectrum with per-frame bar easing.
void drawAudioSpectrum(TFT_eSPI &frameBuffer, uint32_t now,
                       bool streamActive, bool enabled) {
  frameBuffer.fillRect(0, SPECTRUM_TOP, FAT_CAT_PANEL_WIDTH, SPECTRUM_HEIGHT,
                       0x0841);
  frameBuffer.setTextDatum(MC_DATUM);
  frameBuffer.setTextColor(TFT_CYAN, 0x0841);

  if (!enabled) {
#if defined(BOARD_JC3248W535)
    frameBuffer.drawString("SPECTRUM OFF", FAT_CAT_PANEL_WIDTH / 2, 292, 1);
#else
    frameBuffer.drawString("SPECTRUM OFF", FAT_CAT_PANEL_WIDTH / 2, 218, 1);
#endif
    return;
  }

  AudioBlock newestAudioBlock{};
  uint32_t newestAudioBlockAt = 0;
  bool haveAudioBlock = false;
  portENTER_CRITICAL(&audioBlocksMux);
  if (queuedBlocks > 0 &&
      now - lastSpectrumAnalysisAt >= SPECTRUM_REFRESH_INTERVAL_MS) {
    const uint8_t newestBlock =
        (writeBlock + queuedBlocks - 1) % AUDIO_BLOCK_COUNT;
    memcpy(&newestAudioBlock, &audioBlocks[newestBlock],
           sizeof(newestAudioBlock));
    queuedBlocks = 0;
    haveAudioBlock = true;
  }
  newestAudioBlockAt = lastAudioBlockAt;
  portEXIT_CRITICAL(&audioBlocksMux);
  if (haveAudioBlock) {
    analyzeBlock(newestAudioBlock);
    lastSpectrumAnalysisAt = now;
  }

  if (!streamActive || now - newestAudioBlockAt > AUDIO_STALE_MS) {
    for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
      targetHeights[band] = 0;
    }
  }

#if defined(BOARD_JC3248W535)
  constexpr int16_t BAR_WIDTH = 10;
  constexpr int16_t BAR_GAP = 6;
#else
  constexpr int16_t BAR_WIDTH = 7;
  constexpr int16_t BAR_GAP = 4;
#endif
  constexpr int16_t TOTAL_WIDTH =
      SPECTRUM_BANDS * BAR_WIDTH + (SPECTRUM_BANDS - 1) * BAR_GAP;
  constexpr int16_t START_X = (FAT_CAT_PANEL_WIDTH - TOTAL_WIDTH) / 2;
  for (uint8_t band = 0; band < SPECTRUM_BANDS; ++band) {
    const uint8_t target = targetHeights[band];
    displayedHeights[band] =
        target > displayedHeights[band]
            ? min<uint8_t>(target,
                           displayedHeights[band] + SPECTRUM_ATTACK_STEP)
            : max<uint8_t>(target,
                           displayedHeights[band] > SPECTRUM_RELEASE_STEP
                               ? displayedHeights[band] -
                                     SPECTRUM_RELEASE_STEP
                               : target);

    const uint8_t height = displayedHeights[band];
    if (height > 0) {
      const int16_t x = START_X + band * (BAR_WIDTH + BAR_GAP);
      frameBuffer.fillRoundRect(x, SPECTRUM_BASELINE - height, BAR_WIDTH,
                                height, 2, BAND_COLOURS[band]);
    }
  }
}
