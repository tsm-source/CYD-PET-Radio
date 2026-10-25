#include <Arduino.h>
#include <math.h>

#include "timer_alarm.h"

namespace {

constexpr uint32_t ALARM_SAMPLE_RATE = 16000;
constexpr uint32_t NOTE_1_FRAMES = ALARM_SAMPLE_RATE * 18 / 100;
constexpr uint32_t GAP_FRAMES = ALARM_SAMPLE_RATE * 7 / 100;
constexpr uint32_t NOTE_2_FRAMES = ALARM_SAMPLE_RATE * 22 / 100;
constexpr uint32_t NOTE_3_FRAMES = ALARM_SAMPLE_RATE * 30 / 100;
constexpr uint32_t ALARM_TOTAL_FRAMES =
    NOTE_1_FRAMES + GAP_FRAMES + NOTE_2_FRAMES + GAP_FRAMES + NOTE_3_FRAMES;
constexpr uint32_t ENVELOPE_FRAMES = ALARM_SAMPLE_RATE / 100;
constexpr uint32_t FRAMES_PER_WRITE = 512;
constexpr float ALARM_TWO_PI = 6.28318530718f;
constexpr uint32_t ALARM_STACK_SIZE = 4096;
constexpr UBaseType_t ALARM_TASK_PRIORITY = 1;

volatile bool playing = false;
TaskHandle_t alarmTaskHandle = nullptr;

// Generate one sample from the three-note chime, including short note
// envelopes to avoid discontinuities at note boundaries.
int16_t sampleForFrame(uint32_t frame, uint8_t alarmVolume) {
  uint32_t noteFrame = 0;
  uint32_t noteLength = 0;
  float frequency = 0.0f;

  if (frame < NOTE_1_FRAMES) {
    noteFrame = frame;
    noteLength = NOTE_1_FRAMES;
    frequency = 659.25f;
  } else if (frame < NOTE_1_FRAMES + GAP_FRAMES) {
    return 0;
  } else if (frame < NOTE_1_FRAMES + GAP_FRAMES + NOTE_2_FRAMES) {
    noteFrame = frame - NOTE_1_FRAMES - GAP_FRAMES;
    noteLength = NOTE_2_FRAMES;
    frequency = 783.99f;
  } else if (frame <
             NOTE_1_FRAMES + 2 * GAP_FRAMES + NOTE_2_FRAMES) {
    return 0;
  } else {
    noteFrame = frame - NOTE_1_FRAMES - 2 * GAP_FRAMES - NOTE_2_FRAMES;
    noteLength = NOTE_3_FRAMES;
    frequency = 987.77f;
  }

  const uint32_t envelope = min<uint32_t>(
      min<uint32_t>(noteFrame, noteLength - noteFrame - 1),
      ENVELOPE_FRAMES);
  const int32_t amplitude =
      static_cast<int32_t>(12000 * alarmVolume / 21) * envelope /
      ENVELOPE_FRAMES;
  const float angle =
      ALARM_TWO_PI * frequency * static_cast<float>(noteFrame) /
      ALARM_SAMPLE_RATE;
  return static_cast<int16_t>(sinf(angle) * amplitude);
}

// Feed the generated chime to I2S independently of the main UI/audio loop.
struct AlarmTaskParameters {
  i2s_port_t port;
  uint8_t volume;
};

AlarmTaskParameters alarmTaskParameters{};

void alarmTask(void *parameter) {
  const AlarmTaskParameters *taskParameters =
      static_cast<const AlarmTaskParameters *>(parameter);
  const i2s_port_t port = taskParameters->port;
  uint32_t sampleFrame = 0;
  const uint8_t alarmVolume = min<uint8_t>(taskParameters->volume, 21);

  while (playing && sampleFrame < ALARM_TOTAL_FRAMES) {
    uint32_t samples[FRAMES_PER_WRITE];
    const uint32_t frameCount =
        min<uint32_t>(FRAMES_PER_WRITE, ALARM_TOTAL_FRAMES - sampleFrame);
    for (uint32_t index = 0; index < frameCount; ++index) {
      const int16_t sample = sampleForFrame(sampleFrame + index, alarmVolume);
      uint16_t outputSample = static_cast<uint16_t>(sample);
#if !defined(BOARD_JC3248W535)
      // The built-in DAC uses unsigned PCM; external I2S output uses signed PCM.
      outputSample = static_cast<uint16_t>(outputSample + 0x8000);
#endif
      samples[index] = (static_cast<uint32_t>(outputSample) << 16) |
                       outputSample;
    }

    size_t bytesWritten = 0;
    const esp_err_t result =
        i2s_write(port, samples, frameCount * sizeof(uint32_t), &bytesWritten,
                  pdMS_TO_TICKS(100));
    if (result != ESP_OK) {
      Serial.printf("ERROR: timer alarm audio write failed (%d).\n", result);
      playing = false;
      break;
    }
    if (bytesWritten == 0) {
      taskYIELD();
      continue;
    }
    sampleFrame += bytesWritten / sizeof(uint32_t);
  }

  playing = false;
  alarmTaskHandle = nullptr;
  vTaskDelete(nullptr);
}

}  // namespace

// Configure I2S and launch the alarm writer; return false if setup fails.
bool startTimerAlarm(i2s_port_t port, uint8_t musicVolume) {
  if (musicVolume == 0) {
    playing = false;
    return true;
  }
  if (alarmTaskHandle != nullptr) {
    Serial.println("ERROR: timer alarm is already active.");
    return false;
  }

  const esp_err_t result =
      i2s_set_sample_rates(port, ALARM_SAMPLE_RATE);
  if (result != ESP_OK) {
    Serial.printf("ERROR: could not configure timer alarm audio (%d).\n",
                  result);
    playing = false;
    return false;
  }
  const esp_err_t clearResult =
      i2s_zero_dma_buffer(port);
  if (clearResult != ESP_OK) {
    Serial.printf("ERROR: could not clear timer alarm audio buffer (%d).\n",
                  clearResult);
    return false;
  }
  playing = true;
  alarmTaskParameters = {port, musicVolume};
  const BaseType_t taskResult = xTaskCreatePinnedToCore(
      alarmTask, "timerAlarm", ALARM_STACK_SIZE, &alarmTaskParameters,
      ALARM_TASK_PRIORITY, &alarmTaskHandle, 1);
  if (taskResult != pdPASS) {
    alarmTaskHandle = nullptr;
    playing = false;
    Serial.println("ERROR: could not start timer alarm audio task.");
    return false;
  }
  return true;
}

// Stop the alarm task's output immediately and clear any queued DMA samples.
void stopTimerAlarm(i2s_port_t port) {
  playing = false;
  const esp_err_t result = i2s_zero_dma_buffer(port);
  if (result != ESP_OK) {
    Serial.printf("ERROR: could not stop timer alarm audio (%d).\n", result);
  }
}
