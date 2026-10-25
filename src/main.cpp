#include <Arduino.h>
#include <cstring>
#include <Preferences.h>
#include <SPI.h>
#include "radio_display.h"
#include <time.h>
#if defined(BOARD_JC3248W535)
#include <Wire.h>
#else
#include <XPT2046_Touchscreen.h>
#endif
#include <WiFi.h>
#include <Audio.h>
#include <driver/i2s.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <math.h>

#include "audio_spectrum.h"
#include "timer_alarm.h"
#include "audio_compressor.h"
#include "audio_dac.h"
#include "bubble_texts.h"
#include "colors.h"
#include "fat_cat.h"
#include "pets.h"
#include "default_playlist.h"

namespace {

#if defined(BOARD_JC3248W535)
constexpr int16_t DISPLAY_WIDTH = 480;
constexpr int16_t DISPLAY_HEIGHT = 320;
#else
constexpr int16_t DISPLAY_WIDTH = 320;
constexpr int16_t DISPLAY_HEIGHT = 240;
#endif
constexpr uint8_t TOUCH_CALIBRATION_TARGET_COUNT = 5;
#if defined(BOARD_JC3248W535)
constexpr int16_t TOUCH_CALIBRATION_X[TOUCH_CALIBRATION_TARGET_COUNT] = {
    48, 432, 432, 48, 240};
constexpr int16_t TOUCH_CALIBRATION_Y[TOUCH_CALIBRATION_TARGET_COUNT] = {
    36, 36, 284, 284, 160};
constexpr uint8_t TOUCH_CALIBRATION_VERSION = 2;
#else
constexpr int16_t TOUCH_CALIBRATION_X[TOUCH_CALIBRATION_TARGET_COUNT] = {
    48, 271, 271, 48, DISPLAY_WIDTH / 2};
constexpr int16_t TOUCH_CALIBRATION_Y[TOUCH_CALIBRATION_TARGET_COUNT] = {
    36, 36, 203, 203, DISPLAY_HEIGHT / 2};
constexpr uint8_t TOUCH_CALIBRATION_VERSION = 1;
#endif
constexpr int16_t CAT_PANEL_WIDTH = FAT_CAT_PANEL_WIDTH;
constexpr int16_t PET_HEADER_HEIGHT = 32;
#if defined(BOARD_JC3248W535)
constexpr int16_t PET_ANIMATION_HEIGHT = 265 - PET_HEADER_HEIGHT;
#else
constexpr int16_t PET_ANIMATION_HEIGHT = 195 - PET_HEADER_HEIGHT;
#endif
#if defined(BOARD_JC3248W535)
constexpr int16_t HOME_UI_LEFT = 132;
constexpr int16_t HOME_UI_WIDTH = 336;
constexpr int16_t HOME_BOTTOM_BUTTON_Y = 268;
constexpr int16_t HOME_BOTTOM_BUTTON_WIDTH = 160;
constexpr int16_t HOME_BOTTOM_BUTTON_HEIGHT = 42;
constexpr int16_t HOME_PREVIOUS_X = 0;
constexpr int16_t HOME_PREVIOUS_WIDTH = 70;
constexpr int16_t HOME_PLAY_X = 76;
constexpr int16_t HOME_PLAY_WIDTH = 176;
constexpr int16_t HOME_NEXT_X = 258;
constexpr int16_t HOME_NEXT_WIDTH = 70;
constexpr int16_t HOME_PLAYBACK_Y = 153;
constexpr int16_t HOME_PLAYBACK_HEIGHT = 46;
constexpr int16_t HOME_VOLUME_LABEL_Y = 214;
constexpr int16_t HOME_VOLUME_SLIDER_Y = 241;
#else
constexpr int16_t HOME_UI_LEFT = 103;
constexpr int16_t HOME_UI_WIDTH = 210;
constexpr int16_t HOME_BOTTOM_BUTTON_Y = 198;
constexpr int16_t HOME_BOTTOM_BUTTON_WIDTH = 100;
constexpr int16_t HOME_BOTTOM_BUTTON_HEIGHT = 32;
#endif
constexpr int16_t HOME_BOTTOM_BUTTON_GAP =
    HOME_UI_WIDTH - 2 * HOME_BOTTOM_BUTTON_WIDTH;
constexpr int16_t MENU_UI_LEFT = 12;
constexpr int16_t MENU_UI_WIDTH = DISPLAY_WIDTH - 2 * MENU_UI_LEFT;
#if defined(BOARD_JC3248W535)
constexpr int16_t SETTINGS_ITEM_HEIGHT = 48;
constexpr int16_t SETTINGS_ITEM_GAP = 12;
constexpr int16_t SETTINGS_ITEM_START_Y = 42;
#elif defined(BOARD_FREENOVE_FNK0114L)
constexpr int16_t SETTINGS_ITEM_HEIGHT = 32;
constexpr int16_t SETTINGS_ITEM_GAP = 6;
constexpr int16_t SETTINGS_ITEM_START_Y = 39;
#else
constexpr int16_t SETTINGS_ITEM_HEIGHT = 40;
constexpr int16_t SETTINGS_ITEM_GAP = 8;
constexpr int16_t SETTINGS_ITEM_START_Y = 39;
#endif
#if defined(BOARD_JC3248W535)
constexpr int16_t AUDIO_TOGGLE_START_Y = 42;
constexpr int16_t AUDIO_TOGGLE_STEP = 50;
constexpr int16_t AUDIO_TOGGLE_HEIGHT = 44;
constexpr int16_t AUDIO_VOLUME_LABEL_Y = 204;
constexpr int16_t AUDIO_VOLUME_TRACK_Y = 232;
#else
constexpr int16_t AUDIO_TOGGLE_START_Y = 37;
constexpr int16_t AUDIO_TOGGLE_STEP = 32;
constexpr int16_t AUDIO_TOGGLE_HEIGHT = 29;
constexpr int16_t AUDIO_VOLUME_LABEL_Y = 194;
constexpr int16_t AUDIO_VOLUME_TRACK_Y = 220;
#endif
constexpr uint8_t MAX_STATIONS = 20;
constexpr uint8_t STATIONS_PER_PAGE = 4;
constexpr uint8_t STATION_NAME_SIZE = 32;
constexpr uint8_t STATION_URL_SIZE = 192;
constexpr uint16_t CAT_FRAME_INTERVAL_MS = FAT_CAT_FRAME_INTERVAL_MS;
constexpr uint16_t TOUCH_DEBOUNCE_MS = 180;
constexpr uint16_t TOUCH_RELEASE_DEBOUNCE_MS = 80;
constexpr uint32_t WIFI_TIMEOUT_MS = 25000;
constexpr uint32_t WIFI_RECONNECT_DELAY_MS = 30000;
constexpr int AUDIO_INPUT_BUFFER_BYTES = 20000;
constexpr int AUDIO_PSRAM_BUFFER_BYTES = 300000;
constexpr uint32_t STREAM_RETRY_DELAY_MS = 3000;
constexpr uint16_t CLOCK_UPDATE_INTERVAL_MS = 60000;
constexpr uint16_t BITRATE_UPDATE_INTERVAL_MS = 1000;
constexpr uint32_t BUBBLE_VISIBLE_MS = 2000;
constexpr uint32_t MIN_BUBBLE_INTERVAL_MS = 5000;
constexpr uint32_t MAX_BUBBLE_INTERVAL_MS = 15000;
constexpr uint32_t RANDOM_PET_INTERVAL_MS = 30000;
constexpr uint32_t PET_TRANSITION_DURATION_MS = 900;
constexpr uint32_t MIN_DANCE_DURATION_MS = 8000;
constexpr uint32_t MAX_DANCE_DURATION_MS = 16000;
constexpr uint32_t DEFAULT_TIMER_SECONDS = 5 * 60;
constexpr uint32_t MAX_TIMER_SECONDS = 99 * 60 + 59;
constexpr uint32_t TIMER_MUSIC_CUTOFF_MS = 5000;
constexpr uint32_t TIMER_EXPIRED_DISPLAY_MS = 60000;
constexpr int8_t AUDIO_BOOST_DB = 6;
constexpr uint8_t AUDIO_AMP_ENABLE_PIN = 4;
#if defined(BOARD_JC3248W535)
constexpr uint8_t DISPLAY_BACKLIGHT_PIN = 1;
constexpr uint8_t DISPLAY_BACKLIGHT_CHANNEL = 7;
constexpr uint8_t DISPLAY_BRIGHTNESS_MIN = 10;
constexpr uint8_t DISPLAY_BRIGHTNESS_MAX = 100;
constexpr uint8_t DISPLAY_BRIGHTNESS_DEFAULT = 100;
#endif
constexpr char BERLIN_TIMEZONE[] = "CET-1CEST,M3.5.0/2,M10.5.0/3";

constexpr uint16_t PANEL = 0x18E3;
constexpr uint16_t PANEL_LIGHT = 0x2945;
constexpr uint16_t ACCENT = 0x05BF;
constexpr uint8_t THEME_COLOR_COUNT = 10;
constexpr uint8_t DEFAULT_THEME_COLOR = 1;
constexpr uint8_t BUTTON_COLOR_COUNT = 10;
constexpr uint8_t DEFAULT_BUTTON_COLOR = 0;
constexpr uint8_t COLORS_PER_ROW = 5;
#if defined(BOARD_FREENOVE_FNK0114L)
constexpr uint8_t RGB_LED_RED_PIN = 22;
constexpr uint8_t RGB_LED_GREEN_PIN = 16;
constexpr uint8_t RGB_LED_BLUE_PIN = 17;
constexpr uint8_t RGB_LED_RED_CHANNEL = 5;
constexpr uint8_t RGB_LED_GREEN_CHANNEL = 6;
constexpr uint8_t RGB_LED_BLUE_CHANNEL = 7;
constexpr uint8_t RGB_LED_COLOR_COUNT = 5;
constexpr uint32_t RGB_LED_PULSE_PERIOD_MS = 4000;
constexpr uint32_t RGB_LED_UPDATE_INTERVAL_MS = 30;
struct RgbColor {
  uint8_t red;
  uint8_t green;
  uint8_t blue;
};
constexpr RgbColor RGB_LED_COLORS[RGB_LED_COLOR_COUNT] = {
    {255, 36, 18}, {0, 190, 255}, {154, 62, 255}, {255, 132, 18},
    {36, 255, 92}};
#endif
constexpr int16_t HOME_THEME_SWATCH_WIDTH = 36;
constexpr int16_t HOME_THEME_SWATCH_HEIGHT = 16;
constexpr int16_t HOME_THEME_SWATCH_GAP = 6;
constexpr int16_t MENU_THEME_SWATCH_WIDTH = 50;
constexpr int16_t MENU_THEME_SWATCH_HEIGHT = 20;
constexpr int16_t MENU_THEME_SWATCH_GAP = 8;
constexpr uint16_t THEME_COLORS[THEME_COLOR_COUNT] = {
    COLOR_DARK_BLUE, COLOR_DARK_GREY, COLOR_DARK_GREEN, COLOR_DARK_RED,
    COLOR_DARK_SLATE, 0x1865, 0x0925, 0x1901, 0x2843, 0x20E2};
constexpr uint16_t BUTTON_COLORS[BUTTON_COLOR_COUNT] = {
    0x2945, 0x1908, 0x1A28, 0x4148, 0x5182, 0x0372, 0x680A, 0x8A81,
    0x0B26, 0x496F};
constexpr uint16_t TEXT_MUTED = 0x9CF3;
constexpr uint16_t SUCCESS = 0x4FE7;
constexpr uint16_t ERROR_COLOUR = 0xF986;

struct Station {
  char name[STATION_NAME_SIZE]{};
};

struct TouchCalibration {
  float xRawX = 0;
  float xRawY = 0;
  float xOffset = 0;
  float yRawX = 0;
  float yRawY = 0;
  float yOffset = 0;
  bool valid = false;
};

enum class Screen : uint8_t {
  Home,
  Settings,
  AudioSettings,
  DisplaySettings,
  RgbLedSettings,
  Wifi,
  Stations,
  StationEditor,
  Keyboard
};

enum class KeyboardTarget : uint8_t {
  Ssid,
  Password,
  StationName,
  StationUrl
};

enum class PetTransition : uint8_t {
  Entering,
  Idle,
  Exiting
};

enum class TimerState : uint8_t {
  Inactive,
  Ready,
  Running,
  Paused,
  Expired
};

TFT_eSPI display;
// Composite the UI and cat before pushing one complete frame to the TFT.
TFT_eSprite screenFrameBuffer(&display);
TFT_eSprite petPanelFrameBuffer(&display);
TFT_eSprite homeUpdateFrameBuffer(&display);
bool displayFramebufferReady = false;
bool petPanelFramebufferReady = false;
bool homeUpdateFramebufferReady = false;
TFT_eSPI *renderTarget = &display;
int16_t UI_LEFT = HOME_UI_LEFT;
int16_t UI_WIDTH = HOME_UI_WIDTH;
int16_t themeSwatchWidth = HOME_THEME_SWATCH_WIDTH;
int16_t themeSwatchHeight = HOME_THEME_SWATCH_HEIGHT;
int16_t themeSwatchGap = HOME_THEME_SWATCH_GAP;
#if !defined(BOARD_JC3248W535)
SPIClass touchSPI(HSPI);
XPT2046_Touchscreen touch(33, 36);
#else
constexpr uint8_t TOUCH_SDA_PIN = 4;
constexpr uint8_t TOUCH_SCL_PIN = 8;
constexpr uint8_t TOUCH_RESET_PIN = 12;
constexpr uint8_t TOUCH_ADDRESS = 0x3B;
uint16_t lastTouchRawX = 0;
uint16_t lastTouchRawY = 0;
#endif
Preferences preferences;
// DAC channel 2 is GPIO 26, the CYD speaker output.
#if defined(BOARD_JC3248W535)
Audio radioAudio;
#else
Audio radioAudio(true, I2S_DAC_CHANNEL_LEFT_EN);
#endif
#if defined(BOARD_JC3248W535)
constexpr uint32_t RADIO_AUDIO_TASK_STACK_SIZE = 8192;
constexpr UBaseType_t RADIO_AUDIO_TASK_PRIORITY = 4;
constexpr BaseType_t RADIO_AUDIO_TASK_CORE = 0;
SemaphoreHandle_t radioAudioMutex = nullptr;
TaskHandle_t radioAudioTaskHandle = nullptr;
portMUX_TYPE audioCallbackMux = portMUX_INITIALIZER_UNLOCKED;
char pendingStationName[128]{};
char pendingStreamTitle[192]{};
bool pendingStationNameUpdate = false;
bool pendingStreamTitleUpdate = false;
bool pendingStreamEndUpdate = false;
#endif

Station stations[MAX_STATIONS]{};
uint8_t stationCount = 0;
uint8_t selectedStation = 0;
uint8_t stationListPage = 0;
uint8_t volume = 10;
#if defined(BOARD_JC3248W535)
uint8_t displayBrightness = DISPLAY_BRIGHTNESS_DEFAULT;
#endif
uint32_t timerDurationSeconds = DEFAULT_TIMER_SECONDS;
uint32_t timerEndAt = 0;
uint32_t timerPausedRemainingMs = 0;
uint32_t timerExpiredAt = 0;
TimerState timerState = TimerState::Inactive;
bool timerMusicStopped = false;
bool timerShouldResumeMusic = false;
uint8_t themeColorIndex = DEFAULT_THEME_COLOR;
uint8_t buttonColorIndex = DEFAULT_BUTTON_COLOR;
#if defined(BOARD_FREENOVE_FNK0114L)
uint8_t rgbLedColorIndex = 0;
bool rgbLedEnabled = false;
bool rgbLedPulseEnabled = false;
uint32_t lastRgbLedUpdateAt = 0;
#endif
uint16_t backgroundColor = THEME_COLORS[DEFAULT_THEME_COLOR];
uint8_t editorIndex = 0;
uint8_t deleteIndex = 0;
TouchCalibration touchCalibration{};
#if defined(BOARD_FREENOVE_FNK0114L)
TouchCalibration freenoveTouchCalibration{};
#endif
Screen currentScreen = Screen::Home;
Screen keyboardReturnScreen = Screen::Wifi;
KeyboardTarget keyboardTarget = KeyboardTarget::Ssid;

String wifiSsid;
String wifiPassword;
String editName;
String editUrl;
String keyboardValue;
String streamTitle;
String streamStation;
String displayedHomeStation;
String displayedHomeTitleFirst;
String displayedHomeTitleSecond;
String displayedHomeCodec;
String displayedHomeBitrate;
String statusMessage = "Set up Wi-Fi";

bool touchHeld = false;
bool touchReleasePending = false;
uint32_t touchReleaseStartedAt = 0;
bool wifiConnecting = false;
bool playWhenConnected = false;
bool streamActive = false;
bool streamRestartPending = false;
bool showWifiPassword = false;
bool deleteConfirmation = false;
bool stationEditorIsNew = false;
bool audioBoost = false;
bool audioAmpEnabled = false;
bool audioSpectrumEnabled = true;
bool audioCompressorEnabled = false;
bool audioNoiseShaping = false;
bool keyboardUppercase = false;
bool stationListFromSettings = false;
uint8_t keyboardPage = 0;
bool uiDirty = true;
bool audioDirty = false;
bool homeMetadataDirty = false;
bool homeBitrateDirty = false;
bool homeClockDirty = false;
volatile uint8_t wifiDisconnectReason = 0;
uint8_t selectedPetIndex = 0;
uint8_t displayedPetIndex = 0;
int8_t petTitleIndex = -1;
uint32_t wifiConnectStartedAt = 0;
uint32_t streamRestartAt = 0;
uint32_t lastTouchAt = 0;
uint32_t lastCatFrameAt = 0;
uint32_t petTransitionStartedAt = 0;
uint32_t nextDanceChangeAt = 0;
uint32_t lastClockUpdateAt = 0;
uint32_t lastBitrateUpdateAt = 0;
uint32_t lastDisplayedBitrateKbps = UINT32_MAX;
int lastDisplayedCodec = -1;
uint32_t lastPetChangeAt = 0;
uint32_t lastBubbleShownAt = 0;
uint32_t bubbleIntervalMs = MIN_BUBBLE_INTERVAL_MS;
uint8_t bubblePetIndex = UINT8_MAX;
uint8_t bubbleTextIndex = 0;
bool bubbleVisible = false;
bool petAnimationInitialized = false;
PetTransition petTransition = PetTransition::Entering;
PetDance petDance = PetDance::Bounce;
PetOutfit petOutfit = PetOutfit::Tie;

void startWifiConnection();
void startStream();
void markUiDirty();
void setStatus(const String &message);
void updateScreenLayout();
uint8_t stationPageCount();
void showStationPageFor(uint8_t index);

bool timerModeActive() { return timerState != TimerState::Inactive; }

#if defined(BOARD_JC3248W535)
bool lockRadioAudio() {
  if (radioAudioMutex == nullptr ||
      xSemaphoreTake(radioAudioMutex, portMAX_DELAY) != pdTRUE) {
    Serial.println("ERROR: could not lock the radio audio instance.");
    return false;
  }
  return true;
}

void radioAudioTask(void *) {
  for (;;) {
    if (xSemaphoreTake(radioAudioMutex, portMAX_DELAY) == pdTRUE) {
      radioAudio.loop();
      xSemaphoreGive(radioAudioMutex);
    }
    vTaskDelay(1);
  }
}

bool startRadioAudioTask() {
  radioAudioMutex = xSemaphoreCreateMutex();
  if (radioAudioMutex == nullptr) {
    Serial.println("ERROR: could not create the radio audio mutex.");
    return false;
  }
  const BaseType_t result =
      xTaskCreatePinnedToCore(radioAudioTask, "radioAudio",
                              RADIO_AUDIO_TASK_STACK_SIZE, nullptr,
                              RADIO_AUDIO_TASK_PRIORITY,
                              &radioAudioTaskHandle, RADIO_AUDIO_TASK_CORE);
  if (result != pdPASS) {
    Serial.println("ERROR: could not create the radio audio task.");
    vSemaphoreDelete(radioAudioMutex);
    radioAudioMutex = nullptr;
    return false;
  }
  return true;
}

void stopRadioSong() {
  if (lockRadioAudio()) {
    radioAudio.stopSong();
    xSemaphoreGive(radioAudioMutex);
  }
}

void setRadioVolume(uint8_t value) {
  if (lockRadioAudio()) {
    radioAudio.setVolume(value);
    xSemaphoreGive(radioAudioMutex);
  }
}

void setRadioTone(int8_t low, int8_t mid, int8_t high) {
  if (lockRadioAudio()) {
    radioAudio.setTone(low, mid, high);
    xSemaphoreGive(radioAudioMutex);
  }
}

bool connectRadioToHost(const char *url) {
  if (!lockRadioAudio()) {
    return false;
  }
  const bool connected = radioAudio.connecttohost(url);
  xSemaphoreGive(radioAudioMutex);
  return connected;
}

int radioCodec() {
  if (!lockRadioAudio()) {
    return 0;
  }
  const int codec = radioAudio.getCodec();
  xSemaphoreGive(radioAudioMutex);
  return codec;
}

uint32_t radioBitRate() {
  if (!lockRadioAudio()) {
    return 0;
  }
  const uint32_t bitRate = radioAudio.getBitRate();
  xSemaphoreGive(radioAudioMutex);
  return bitRate;
}

const char *radioCodecName() {
  if (!lockRadioAudio()) {
    return "";
  }
  const char *name = radioAudio.getCodecname();
  xSemaphoreGive(radioAudioMutex);
  return name;
}

i2s_port_t radioI2sPort() {
  if (!lockRadioAudio()) {
    return I2S_NUM_0;
  }
  const i2s_port_t port = static_cast<i2s_port_t>(radioAudio.getI2sPort());
  xSemaphoreGive(radioAudioMutex);
  return port;
}

void applyPendingAudioCallbacks() {
  char station[sizeof(pendingStationName)];
  char title[sizeof(pendingStreamTitle)];
  bool stationChanged = false;
  bool titleChanged = false;
  bool streamEnded = false;
  portENTER_CRITICAL(&audioCallbackMux);
  stationChanged = pendingStationNameUpdate;
  titleChanged = pendingStreamTitleUpdate;
  streamEnded = pendingStreamEndUpdate;
  if (stationChanged) {
    strlcpy(station, pendingStationName, sizeof(station));
    pendingStationNameUpdate = false;
  }
  if (titleChanged) {
    strlcpy(title, pendingStreamTitle, sizeof(title));
    pendingStreamTitleUpdate = false;
  }
  if (streamEnded) {
    pendingStreamEndUpdate = false;
  }
  portEXIT_CRITICAL(&audioCallbackMux);

  if (stationChanged) {
    streamStation = station;
    audioDirty = true;
  }
  if (titleChanged) {
    streamTitle = title;
    audioDirty = true;
  }
  if (streamEnded && streamActive) {
    streamActive = false;
    streamRestartPending = true;
    streamRestartAt = millis();
    setStatus("Stream ended");
  }
}
#else
void stopRadioSong() { radioAudio.stopSong(); }
void setRadioVolume(uint8_t value) { radioAudio.setVolume(value); }
void setRadioTone(int8_t low, int8_t mid, int8_t high) {
  radioAudio.setTone(low, mid, high);
}
bool connectRadioToHost(const char *url) {
  return radioAudio.connecttohost(url);
}
int radioCodec() { return radioAudio.getCodec(); }
uint32_t radioBitRate() { return radioAudio.getBitRate(); }
const char *radioCodecName() { return radioAudio.getCodecname(); }
i2s_port_t radioI2sPort() {
  return static_cast<i2s_port_t>(radioAudio.getI2sPort());
}
void applyPendingAudioCallbacks() {}
#endif

// Return the duration currently shown, accounting for ready, paused, and
// running states while keeping the countdown safe across millis() rollover.
uint32_t timerRemainingMs(uint32_t now) {
  if (timerState == TimerState::Ready) {
    return timerDurationSeconds * 1000;
  }
  if (timerState == TimerState::Paused) {
    return timerPausedRemainingMs;
  }
  if (timerState != TimerState::Running) {
    return 0;
  }
  const int32_t remaining = static_cast<int32_t>(timerEndAt - now);
  return remaining > 0 ? static_cast<uint32_t>(remaining) : 0;
}

// Leave timer mode, stop the alarm, restore the pet animation, and resume any
// stream that the timer itself stopped.
void exitTimerMode(uint32_t now) {
  const bool resumeMusic = timerShouldResumeMusic;
  timerState = TimerState::Inactive;
  timerMusicStopped = false;
  timerShouldResumeMusic = false;
  stopTimerAlarm(radioI2sPort());
  lastPetChangeAt = now;
  petAnimationInitialized = false;
  petTransition = PetTransition::Entering;
  petTransitionStartedAt = now;
  if (resumeMusic) {
    if (WiFi.status() == WL_CONNECTED) {
      startStream();
    } else if (!wifiSsid.isEmpty()) {
      startWifiConnection();
    }
  }
  markUiDirty();
}

// Stop playback at the cutoff and remember whether it should resume later.
void stopMusicForTimer() {
  timerMusicStopped = true;
  streamRestartPending = false;
  if (streamActive) {
    timerShouldResumeMusic = true;
    stopRadioSong();
    streamActive = false;
    streamTitle = "";
    streamStation = "";
    setStatus("Timer: music stopped");
  }
}

// Start or resume the countdown from the selected duration.
void startTimer(uint32_t now) {
  uint32_t remaining = timerState == TimerState::Paused
                           ? timerPausedRemainingMs
                           : timerDurationSeconds * 1000;
  if (remaining == 0) {
    setStatus("Set timer duration first");
    markUiDirty();
    return;
  }
  timerEndAt = now + remaining;
  timerState = TimerState::Running;
  markUiDirty();
}

// Enter the expired state, stop music, and start the alarm chime.
void expireTimer(uint32_t now) {
  stopMusicForTimer();
  timerState = TimerState::Expired;
  timerExpiredAt = now;
  if (!startTimerAlarm(radioI2sPort(), volume) && volume > 0) {
    setStatus("Timer finished; alarm audio unavailable");
  } else {
    setStatus("Timer finished");
  }
  markUiDirty();
}

// Pause a running timer or confirm its expired state.
void stopOrConfirmTimer(uint32_t now) {
  if (timerState == TimerState::Running) {
    timerPausedRemainingMs = timerRemainingMs(now);
    if (timerPausedRemainingMs == 0) {
      expireTimer(now);
      return;
    }
    timerState = TimerState::Paused;
  } else if (timerState == TimerState::Expired) {
    exitTimerMode(now);
    return;
  }
  markUiDirty();
}

// Adjust and persist the configured duration or paused remaining time.
void changeTimerDuration(int32_t deltaSeconds) {
  uint32_t currentSeconds =
      timerState == TimerState::Paused
          ? (timerPausedRemainingMs + 999) / 1000
          : timerDurationSeconds;
  const int64_t adjusted =
      static_cast<int64_t>(currentSeconds) + deltaSeconds;
  timerDurationSeconds = static_cast<uint32_t>(
      adjusted < 0 ? 0
                   : min<int64_t>(adjusted, MAX_TIMER_SECONDS));
  if (timerState == TimerState::Paused) {
    timerPausedRemainingMs = timerDurationSeconds * 1000;
  }
  if (preferences.putUInt("timerSecs", timerDurationSeconds) == 0) {
    Serial.println("ERROR: could not save timer duration.");
    setStatus("Could not save timer duration");
  }
  markUiDirty();
}

// Toggle timer mode from the home-screen TIMER control.
void pressTimerButton(uint32_t now) {
  if (timerState == TimerState::Inactive) {
    timerState = TimerState::Ready;
  } else {
    exitTimerMode(now);
    return;
  }
  markUiDirty();
}

// Enforce the pre-expiry music cutoff and the post-expiry display timeout.
void updateTimer(uint32_t now) {
  if (timerState == TimerState::Running) {
    const uint32_t remaining = timerRemainingMs(now);
    if (remaining <= TIMER_MUSIC_CUTOFF_MS && !timerMusicStopped) {
      stopMusicForTimer();
    }
    if (remaining == 0) {
      expireTimer(now);
    }
  } else if (timerState == TimerState::Expired &&
             now - timerExpiredAt >= TIMER_EXPIRED_DISPLAY_MS) {
    exitTimerMode(now);
  }

}

// Select a different pet without repeating the currently selected pet.
void chooseDifferentPet() {
  uint8_t nextIndex = esp_random() % (PET_COUNT - 1);
  if (nextIndex >= selectedPetIndex) {
    ++nextIndex;
  }
  selectedPetIndex = nextIndex;
}

// Choose another bubble message so successive appearances are not repetitive.
void chooseDifferentBubbleText() {
  uint8_t nextIndex = esp_random() % 9;
  if (nextIndex >= bubbleTextIndex) {
    ++nextIndex;
  }
  bubbleTextIndex = nextIndex;
}

uint32_t randomBubbleInterval() {
  return MIN_BUBBLE_INTERVAL_MS +
         esp_random() % (MAX_BUBBLE_INTERVAL_MS - MIN_BUBBLE_INTERVAL_MS + 1);
}

PetDance randomDifferentDance(PetDance previous) {
  uint8_t choice = esp_random() % 2;
  const uint8_t previousIndex = static_cast<uint8_t>(previous);
  if (choice >= previousIndex) {
    ++choice;
  }
  return static_cast<PetDance>(choice);
}

uint32_t randomDanceDuration() {
  return MIN_DANCE_DURATION_MS +
         esp_random() % (MAX_DANCE_DURATION_MS - MIN_DANCE_DURATION_MS + 1);
}

void updatePetAnimation(uint32_t now) {
  const uint8_t wantedPetIndex = selectedPetIndex;
  if (!petAnimationInitialized) {
    displayedPetIndex = wantedPetIndex;
    petDance = static_cast<PetDance>(esp_random() % 3);
    petOutfit = static_cast<PetOutfit>(esp_random() % 4);
    petTransition = PetTransition::Entering;
    petTransitionStartedAt = now;
    nextDanceChangeAt = now + randomDanceDuration();
    petAnimationInitialized = true;
    return;
  }

  if (petTransition == PetTransition::Exiting &&
      now - petTransitionStartedAt >= PET_TRANSITION_DURATION_MS) {
    displayedPetIndex = wantedPetIndex;
    petDance = static_cast<PetDance>(esp_random() % 3);
    petOutfit = static_cast<PetOutfit>(esp_random() % 4);
    petTransition = PetTransition::Entering;
    petTransitionStartedAt = now;
    nextDanceChangeAt = now + randomDanceDuration();
  } else if (petTransition == PetTransition::Entering &&
             now - petTransitionStartedAt >= PET_TRANSITION_DURATION_MS) {
    petTransition = PetTransition::Idle;
    if (displayedPetIndex != wantedPetIndex) {
      petTransition = PetTransition::Exiting;
      petTransitionStartedAt = now;
    } else {
      lastPetChangeAt = now;
    }
  } else if (petTransition == PetTransition::Idle) {
    if (displayedPetIndex != wantedPetIndex) {
      petTransition = PetTransition::Exiting;
      petTransitionStartedAt = now;
    } else if (now >= nextDanceChangeAt) {
      petDance = randomDifferentDance(petDance);
      nextDanceChangeAt = now + randomDanceDuration();
    }
  }
}

// Convert the current enter/exit animation phase into its horizontal offset.
int16_t petTransitionOffset(uint32_t now) {
  if (petTransition == PetTransition::Idle) {
    return 0;
  }
  const uint32_t elapsed = min<uint32_t>(
      now - petTransitionStartedAt, PET_TRANSITION_DURATION_MS);
  const int16_t travel = static_cast<int16_t>(
      elapsed * FAT_CAT_PANEL_WIDTH / PET_TRANSITION_DURATION_MS);
  return petTransition == PetTransition::Exiting
             ? -travel
             : travel - FAT_CAT_PANEL_WIDTH;
}

// Shift only the pet panel's pixels to animate its slide transition.
void shiftPetPanel(TFT_eSprite &frameBuffer, int16_t offset) {
  if (offset == 0) {
    return;
  }
  const int16_t distance = abs(offset);
  if (distance >= FAT_CAT_PANEL_WIDTH) {
    frameBuffer.fillRect(0, PET_HEADER_HEIGHT, FAT_CAT_PANEL_WIDTH,
                         PET_ANIMATION_HEIGHT, 0x0841);
    return;
  }

  auto *pixels = static_cast<uint8_t *>(frameBuffer.getPointer());
  if (pixels == nullptr) {
    Serial.println("ERROR: missing display frame buffer during pet transition.");
    return;
  }
  for (int16_t y = PET_HEADER_HEIGHT; y < 195; ++y) {
    uint8_t *row = pixels + y * DISPLAY_WIDTH;
    if (offset > 0) {
      memmove(row + distance, row, FAT_CAT_PANEL_WIDTH - distance);
    } else {
      memmove(row, row + distance, FAT_CAT_PANEL_WIDTH - distance);
    }
  }
  if (offset > 0) {
    frameBuffer.fillRect(0, PET_HEADER_HEIGHT, distance,
                         PET_ANIMATION_HEIGHT, 0x0841);
  } else {
    frameBuffer.fillRect(FAT_CAT_PANEL_WIDTH - distance, PET_HEADER_HEIGHT,
                         distance, PET_ANIMATION_HEIGHT, 0x0841);
  }
}

// Show a new pet's message immediately, then cycle through timed appearances.
void updateBubbleText(uint32_t now) {
  if (selectedPetIndex != bubblePetIndex) {
    bubblePetIndex = selectedPetIndex;
    bubbleTextIndex = esp_random() % 10;
    bubbleVisible = true;
    lastBubbleShownAt = now;
    bubbleIntervalMs = randomBubbleInterval();
    markUiDirty();
    return;
  }

  if (bubbleVisible && now - lastBubbleShownAt >= BUBBLE_VISIBLE_MS) {
    bubbleVisible = false;
    markUiDirty();
  } else if (!bubbleVisible &&
             now - lastBubbleShownAt >= bubbleIntervalMs) {
    chooseDifferentBubbleText();
    bubbleVisible = true;
    lastBubbleShownAt = now;
    bubbleIntervalMs = randomBubbleInterval();
    markUiDirty();
  }
}

void drawSpeechBubble(TFT_eSPI &frameBuffer, const char *text) {
  constexpr int16_t BUBBLE_X = 4;
  constexpr int16_t BUBBLE_Y = 48;
  constexpr int16_t BUBBLE_WIDTH = 88;
  constexpr int16_t BUBBLE_HEIGHT = 30;
  frameBuffer.fillRoundRect(BUBBLE_X, BUBBLE_Y, BUBBLE_WIDTH, BUBBLE_HEIGHT,
                            6, TFT_WHITE);
  frameBuffer.drawRoundRect(BUBBLE_X, BUBBLE_Y, BUBBLE_WIDTH, BUBBLE_HEIGHT,
                            6, 0x4208);
  frameBuffer.fillTriangle(39, 76, 49, 76, 44, 84, TFT_WHITE);
  frameBuffer.drawLine(39, 76, 44, 84, 0x4208);
  frameBuffer.drawLine(49, 76, 44, 84, 0x4208);
  frameBuffer.setTextDatum(MC_DATUM);
  frameBuffer.setTextColor(TFT_BLACK, TFT_WHITE);
  frameBuffer.drawString(text, 48, 63, 1);
}

void drawTimerAdjustButton(int16_t x, int16_t y, bool increase,
                           const char *label) {
  renderTarget->fillRoundRect(x, y, 43, 25, 5,
                              BUTTON_COLORS[buttonColorIndex]);
  renderTarget->drawRoundRect(x, y, 43, 25, 5, 0x4208);
  if (increase) {
    renderTarget->fillTriangle(x + 7, y + 13, x + 15, y + 13, x + 11,
                               y + 7, TFT_WHITE);
  } else {
    renderTarget->fillTriangle(x + 7, y + 11, x + 15, y + 11, x + 11,
                               y + 17, TFT_WHITE);
  }
  renderTarget->setTextDatum(MC_DATUM);
  renderTarget->setTextColor(TFT_WHITE, BUTTON_COLORS[buttonColorIndex]);
  renderTarget->drawString(label, x + 30, y + 13, 1);
}

void drawTimerActionButton(const char *label) {
  renderTarget->fillRoundRect(8, 158, 80, 27, 6,
                              BUTTON_COLORS[buttonColorIndex]);
  renderTarget->drawRoundRect(8, 158, 80, 27, 6, 0x4208);
  renderTarget->setTextDatum(MC_DATUM);
  renderTarget->setTextColor(TFT_WHITE, BUTTON_COLORS[buttonColorIndex]);
#if defined(BOARD_JC3248W535)
  renderTarget->setTextSize(1);
  renderTarget->drawString(label, 48, 172, 2);
  renderTarget->setTextSize(1);
#else
  renderTarget->drawString(label, 48, 172, 2);
#endif
}

// Render timer controls, or replace the panel with the pulsing expiry screen.
void drawTimerPanel(uint32_t now) {
  if (timerState == TimerState::Expired) {
    const uint16_t phase = now % 1200;
    const uint16_t pulse = phase < 600 ? phase : 1200 - phase;
    const uint8_t green = 48 + pulse * 176 / 600;
    const uint16_t orange = renderTarget->color565(255, green, 0);
    renderTarget->fillRect(0, 0,
#if defined(BOARD_JC3248W535)
                           CAT_PANEL_WIDTH,
#else
                           DISPLAY_WIDTH / 2,
#endif
                           DISPLAY_HEIGHT, orange);
    renderTarget->setTextDatum(MC_DATUM);
    renderTarget->setTextColor(TFT_WHITE, orange);
#if defined(BOARD_JC3248W535)
    renderTarget->setTextSize(1);
    renderTarget->drawString("Time", CAT_PANEL_WIDTH / 2, 110, 2);
    renderTarget->drawString("is", CAT_PANEL_WIDTH / 2, 155, 2);
    renderTarget->drawString("up", CAT_PANEL_WIDTH / 2, 200, 2);
#else
    renderTarget->setTextSize(3);
    renderTarget->drawString("Time", DISPLAY_WIDTH / 4, 72, 2);
    renderTarget->drawString("is", DISPLAY_WIDTH / 4, 120, 2);
    renderTarget->drawString("up", DISPLAY_WIDTH / 4, 168, 2);
#endif
    renderTarget->setTextSize(1);
    return;
  }

  renderTarget->fillRect(0, 0, CAT_PANEL_WIDTH, 195, 0x0841);
  renderTarget->setTextDatum(MC_DATUM);
  renderTarget->setTextColor(TFT_CYAN, 0x0841);
  switch (timerState) {
    case TimerState::Ready:
      renderTarget->drawString("SET TIMER", 48, 25, 1);
      break;
    case TimerState::Running:
      renderTarget->drawString("RUNNING", 48, 25, 1);
      break;
    case TimerState::Paused:
      renderTarget->drawString("PAUSED", 48, 25, 1);
      break;
    case TimerState::Expired:
      return;
    case TimerState::Inactive:
      return;
  }

  if (timerState == TimerState::Ready || timerState == TimerState::Paused) {
    drawTimerAdjustButton(4, 39, true, "1m");
    drawTimerAdjustButton(49, 39, true, "10s");
  }

  const uint32_t remainingSeconds = (timerRemainingMs(now) + 999) / 1000;
  renderTarget->setTextColor(TFT_WHITE, 0x0841);
  if (remainingSeconds < 60) {
    char seconds[3];
    snprintf(seconds, sizeof(seconds), "%02lu",
             static_cast<unsigned long>(remainingSeconds));
#if defined(BOARD_JC3248W535)
    renderTarget->setTextSize(1);
    renderTarget->drawString(seconds, 48, 91, 3);
#else
    renderTarget->setTextSize(3);
    renderTarget->drawString(seconds, 48, 91, 2);
#endif
    renderTarget->setTextSize(1);
  } else {
    char countdown[6];
    snprintf(countdown, sizeof(countdown), "%02lu:%02lu",
             static_cast<unsigned long>(remainingSeconds / 60),
             static_cast<unsigned long>(remainingSeconds % 60));
#if defined(BOARD_JC3248W535)
    renderTarget->setTextSize(1);
    renderTarget->drawString(countdown, 48, 91, 2);
#else
    renderTarget->setTextSize(2);
    renderTarget->drawString(countdown, 48, 91, 2);
#endif
    renderTarget->setTextSize(1);
  }

  if (timerState == TimerState::Ready || timerState == TimerState::Paused) {
    drawTimerAdjustButton(4, 123, false, "1m");
    drawTimerAdjustButton(49, 123, false, "10s");
    drawTimerActionButton("START");
  } else {
    drawTimerActionButton("STOP");
  }
}

// Render the pet or timer panel, then update shared overlays and push the
// appropriate screen region to the display.
void drawCatFrame(uint32_t now, bool pushFullFrame = false) {
  const bool timerExpired = timerState == TimerState::Expired;
  const bool usePetPanelFramebuffer = petPanelFramebufferReady &&
                                      !displayFramebufferReady;
  if (usePetPanelFramebuffer) {
    renderTarget = &petPanelFrameBuffer;
  } else if (displayFramebufferReady) {
    renderTarget = &screenFrameBuffer;
  }
  if (timerModeActive()) {
    petTitleIndex = -1;
    drawTimerPanel(now);
  } else {
    const Pet &pet = petList[displayedPetIndex];
    const bool titleAreaCleared =
        pushFullFrame && (displayFramebufferReady || !petPanelFramebufferReady);
    if (titleAreaCleared || petTitleIndex != displayedPetIndex) {
      renderTarget->fillRect(0, 0, CAT_PANEL_WIDTH, PET_HEADER_HEIGHT, 0x0841);
      renderTarget->setTextDatum(MC_DATUM);
      renderTarget->setTextColor(TFT_CYAN, 0x0841);
      renderTarget->drawString(pet.animalName, CAT_PANEL_WIDTH / 2, 18, 2);
      renderTarget->drawFastHLine(0, PET_HEADER_HEIGHT - 1,
                                  CAT_PANEL_WIDTH, 0x4208);
      petTitleIndex = displayedPetIndex;
    }
    renderTarget->fillRect(0, PET_HEADER_HEIGHT, CAT_PANEL_WIDTH,
                           PET_ANIMATION_HEIGHT, 0x0841);
    renderTarget->setViewport(0, PET_HEADER_HEIGHT, CAT_PANEL_WIDTH,
                              PET_ANIMATION_HEIGHT, false);
#if defined(BOARD_JC3248W535)
    renderTarget->setPetDrawingScale(true);
#endif
    if (pet.drawingIndex < 0) {
      drawFatCat(*renderTarget, now, petDance, petOutfit);
    } else {
      drawPet(*renderTarget, now, pet, petDance, petOutfit);
    }
#if defined(BOARD_JC3248W535)
    renderTarget->setPetDrawingScale(false);
#endif
    if (bubbleVisible) {
      drawSpeechBubble(*renderTarget,
                       bubbleTexts[bubblePetIndex][bubbleTextIndex]);
    }
    renderTarget->resetViewport();
  }
  if (!timerExpired) {
    if (displayFramebufferReady) {
      shiftPetPanel(screenFrameBuffer, petTransitionOffset(now));
    }
    drawAudioSpectrum(*renderTarget, now, streamActive,
                      audioSpectrumEnabled);
    const bool wifiConnected = WiFi.status() == WL_CONNECTED;
    const String wifiLabel =
        wifiConnected ? "Wi-Fi " + String(WiFi.RSSI()) + " dBm" : "Wi-Fi OFF";
    renderTarget->setTextDatum(TL_DATUM);
    renderTarget->setTextColor(wifiConnected ? SUCCESS : TEXT_MUTED, 0x0841);
    renderTarget->drawString(wifiLabel, 2, 2, 1);
  }
  if (displayFramebufferReady) {
    if (pushFullFrame) {
      screenFrameBuffer.pushSprite(0, 0);
    } else {
      screenFrameBuffer.pushSprite(0, 0, 0, 0,
                                   timerExpired ? DISPLAY_WIDTH / 2
                                                : CAT_PANEL_WIDTH,
                                   DISPLAY_HEIGHT);
    }
  } else if (usePetPanelFramebuffer) {
    petPanelFrameBuffer.pushSprite(0, 0);
  }
  renderTarget = &display;
}

bool pointInRect(int16_t x, int16_t y, int16_t left, int16_t top,
                 int16_t width, int16_t height) {
  return x >= left && x < left + width && y >= top && y < top + height;
}

bool pointOnMenuBackButton(int16_t x, int16_t y) {
  return pointInRect(x, y, UI_LEFT + UI_WIDTH - 56, 0, 56, 40);
}

void markUiDirty() {
  uiDirty = true;
}

String fittedText(const String &source, int16_t maxWidth, uint8_t font = 2) {
  TFT_eSPI &display = *renderTarget;
  String result = source;
  if (display.textWidth(result, font) <= maxWidth) {
    return result;
  }

  while (result.length() > 3 &&
         display.textWidth(result + "...", font) > maxWidth) {
    result.remove(result.length() - 1);
  }
  return result + "...";
}

struct TwoLineText {
  String first;
  String second;
};

TwoLineText wrappedText(const String &source, int16_t maxWidth,
                        uint8_t font = 2) {
  TwoLineText lines;
  String remaining = source;
  for (uint8_t line = 0; line < 2 && !remaining.isEmpty(); ++line) {
    int16_t fitLength = remaining.length();
    while (fitLength > 0 &&
           renderTarget->textWidth(remaining.substring(0, fitLength), font) >
               maxWidth) {
      --fitLength;
    }

    if (fitLength == remaining.length()) {
      (line == 0 ? lines.first : lines.second) = remaining;
      break;
    }
    if (fitLength == 0) {
      (line == 0 ? lines.first : lines.second) =
          fittedText(remaining, maxWidth, font);
      break;
    }

    int16_t splitAt = remaining.lastIndexOf(' ', fitLength - 1);
    if (splitAt <= 0) {
      splitAt = fitLength;
    }
    (line == 0 ? lines.first : lines.second) =
        remaining.substring(0, splitAt);
    remaining = remaining.substring(splitAt);
    while (remaining.startsWith(" ")) {
      remaining.remove(0, 1);
    }
    if (line == 1 && !remaining.isEmpty()) {
      lines.second = fittedText(lines.second + " " + remaining, maxWidth, font);
    }
  }
  return lines;
}

// Mask saved credentials unless the user explicitly enables password display.
String passwordForDisplay(const String &password) {
  if (showWifiPassword) {
    return password;
  }

  String masked;
  masked.reserve(password.length());
  for (size_t i = 0; i < password.length(); ++i) {
    masked += '*';
  }
  return masked;
}

// Return the active keyboard row for the alphabetic or symbol page.
String keyboardRow(uint8_t row) {
  if (keyboardPage == 1) {
    const String symbolRows[] = {"1234567890", "!@#$%^&*()",
                                 "-_./:;?+="};
    return symbolRows[row];
  }
  if (keyboardPage == 2) {
    const String symbolRows[] = {"[]{}<>~", "`\\|,", "'\""};
    return symbolRows[row];
  }
  const String letterRows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  return letterRows[row];
}

bool wideKeyboardLayout() {
  return UI_WIDTH == MENU_UI_WIDTH;
}

int16_t keyboardKeyGap() {
  return wideKeyboardLayout() ? 2 : 1;
}

int16_t keyboardKeyHeight() {
#if defined(BOARD_JC3248W535)
  return 42;
#else
  return wideKeyboardLayout() ? 34 : 25;
#endif
}

int16_t keyboardRowY(uint8_t row) {
#if defined(BOARD_JC3248W535)
  return 61 + row * 46;
#else
  return wideKeyboardLayout() ? 65 + row * 37 : 66 + row * 29;
#endif
}

int16_t keyboardControlY() {
#if defined(BOARD_JC3248W535)
  return 203;
#else
  return wideKeyboardLayout() ? 177 : 156;
#endif
}

int16_t keyboardControlHeight() {
#if defined(BOARD_JC3248W535)
  return 36;
#else
  return wideKeyboardLayout() ? 28 : 24;
#endif
}

int16_t keyboardActionY() {
#if defined(BOARD_JC3248W535)
  return 249;
#else
  return wideKeyboardLayout() ? 211 : 192;
#endif
}

int16_t keyboardActionHeight() {
#if defined(BOARD_JC3248W535)
  return 46;
#else
  return wideKeyboardLayout() ? 25 : 31;
#endif
}

int16_t keyboardKeyWidth(const String &keys) {
  const int16_t gap = keyboardKeyGap();
  const int16_t availableWidth = UI_WIDTH - (keys.length() - 1) * gap;
  return min<int16_t>(wideKeyboardLayout() ? 48 : 32,
                      availableWidth / keys.length());
}

void keyboardControlBounds(uint8_t index, int16_t &x, int16_t &width) {
  if (wideKeyboardLayout()) {
    constexpr int16_t controlWidths[] = {64, 64, 76, 64};
    const int16_t controlsWidth = 64 + 6 + 64 + 6 + 76 + 6 + 64;
    const int16_t controlsLeft = UI_LEFT + (UI_WIDTH - controlsWidth) / 2;
    x = controlsLeft;
    for (uint8_t i = 0; i < index; ++i) {
      x += controlWidths[i] + 6;
    }
    width = controlWidths[index];
    return;
  }

  constexpr int16_t controlLefts[] = {0, 43, 95, 165};
  constexpr int16_t controlWidths[] = {39, 48, 66, 45};
  x = UI_LEFT + controlLefts[index];
  width = controlWidths[index];
}

uint16_t primaryTextColor() {
  return TFT_WHITE;
}

int16_t themeSwatchX(uint8_t index) {
  const int16_t paletteWidth =
      COLORS_PER_ROW * themeSwatchWidth +
      (COLORS_PER_ROW - 1) * themeSwatchGap;
  return UI_LEFT + (UI_WIDTH - paletteWidth) / 2 +
         index * (themeSwatchWidth + themeSwatchGap);
}

void updateScreenLayout() {
  if (currentScreen == Screen::Home) {
    UI_LEFT = HOME_UI_LEFT;
    UI_WIDTH = HOME_UI_WIDTH;
    themeSwatchWidth = HOME_THEME_SWATCH_WIDTH;
    themeSwatchHeight = HOME_THEME_SWATCH_HEIGHT;
    themeSwatchGap = HOME_THEME_SWATCH_GAP;
    return;
  }
  UI_LEFT = MENU_UI_LEFT;
  UI_WIDTH = MENU_UI_WIDTH;
  themeSwatchWidth = MENU_THEME_SWATCH_WIDTH;
  themeSwatchHeight = MENU_THEME_SWATCH_HEIGHT;
  themeSwatchGap = MENU_THEME_SWATCH_GAP;
}

// Apply a validated background palette selection and persist it for next boot.
void setThemeColor(uint8_t index) {
  if (index >= THEME_COLOR_COUNT || index == themeColorIndex) {
    return;
  }

  themeColorIndex = index;
  backgroundColor = THEME_COLORS[index];
  if (preferences.putUChar("theme", themeColorIndex) == 0) {
    Serial.println("ERROR: could not save background color setting.");
    statusMessage = "Could not save background color";
  }
  markUiDirty();
}

// Apply and persist the button palette used throughout the interface.
void setButtonColor(uint8_t index) {
  if (index >= BUTTON_COLOR_COUNT || index == buttonColorIndex) {
    return;
  }

  buttonColorIndex = index;
  if (preferences.putUChar("buttonColor", buttonColorIndex) == 0) {
    Serial.println("ERROR: could not save button color setting.");
    setStatus("Could not save button color");
    return;
  }
  markUiDirty();
}

#if defined(BOARD_JC3248W535)
void applyDisplayBrightness(uint8_t percent) {
  const uint32_t duty =
      static_cast<uint32_t>(percent) * 255 / DISPLAY_BRIGHTNESS_MAX;
  ledcWrite(DISPLAY_BACKLIGHT_CHANNEL, duty);
}

void setDisplayBrightness(uint8_t percent) {
  percent = constrain(percent, DISPLAY_BRIGHTNESS_MIN,
                      DISPLAY_BRIGHTNESS_MAX);
  if (percent == displayBrightness) {
    return;
  }

  displayBrightness = percent;
  applyDisplayBrightness(displayBrightness);
  if (preferences.putUChar("brightness", displayBrightness) == 0) {
    Serial.println("ERROR: could not save display brightness.");
    setStatus("Could not save brightness");
    return;
  }
  markUiDirty();
}

void initializeDisplayBacklight() {
  ledcSetup(DISPLAY_BACKLIGHT_CHANNEL, 5000, 8);
  ledcAttachPin(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_CHANNEL);
  applyDisplayBrightness(displayBrightness);
}
#endif

void drawText(const String &text, int16_t x, int16_t y, uint8_t font = 2,
              uint16_t colour = TFT_WHITE) {
  TFT_eSPI &display = *renderTarget;
  display.setTextDatum(TL_DATUM);
  display.setTextColor(colour == TFT_WHITE ? primaryTextColor() : colour,
                       backgroundColor);
  display.drawString(text, x, y, font);
}

void drawButton(int16_t x, int16_t y, int16_t width, int16_t height,
                const String &label, uint16_t fill = PANEL_LIGHT,
                uint16_t colour = TFT_WHITE, uint8_t font = 2,
                bool outline = true) {
  TFT_eSPI &display = *renderTarget;
  if (fill == PANEL_LIGHT) {
    fill = BUTTON_COLORS[buttonColorIndex];
  }
  display.fillRoundRect(x, y, width, height, 7, fill);
  if (outline) {
    display.drawRoundRect(x, y, width, height, 7, 0x4208);
  }
  display.setTextDatum(MC_DATUM);
  display.setTextColor(colour == TFT_WHITE ? primaryTextColor() : colour,
                       fill);
  display.drawString(label, x + width / 2, y + height / 2, font);
}

void drawPanelHeader(const String &title) {
  TFT_eSPI &display = *renderTarget;
#if defined(BOARD_JC3248W535)
  const int16_t screenWidth = jc3248w535::DISPLAY_WIDTH;
  const int16_t screenHeight = jc3248w535::DISPLAY_HEIGHT;
#else
  const int16_t screenWidth = DISPLAY_WIDTH;
  const int16_t screenHeight = DISPLAY_HEIGHT;
#endif
  const int16_t panelLeft =
      currentScreen == Screen::Home ? CAT_PANEL_WIDTH : 0;
  display.fillRect(panelLeft, 0, screenWidth - panelLeft, screenHeight,
                   backgroundColor);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TFT_CYAN, backgroundColor);
  display.drawString(title, UI_LEFT, 9, 2);
  const int16_t lineLeft =
      currentScreen == Screen::Home ? CAT_PANEL_WIDTH : UI_LEFT;
  const int16_t lineWidth =
      currentScreen == Screen::Home ? screenWidth - CAT_PANEL_WIDTH
                                    : UI_WIDTH;
  display.drawFastHLine(lineLeft, 31, lineWidth, 0x4208);
}

void drawMenuBackButton() {
  drawButton(UI_LEFT + UI_WIDTH - 56, 3, 56, 32, "BACK", PANEL_LIGHT,
             TFT_WHITE, 1);
}

// Select the most useful Wi-Fi status text for the current connection state.
String wifiStatusText() {
  if (wifiConnecting) {
    return "Connecting to Wi-Fi...";
  }
  if (WiFi.status() == WL_CONNECTED) {
    return WiFi.localIP().toString();
  }
  if (wifiSsid.isEmpty()) {
    return "Wi-Fi not configured";
  }
  return statusMessage;
}

void drawHomeTop(int16_t originX, int16_t originY);
void drawHomeMetadata(int16_t originX, int16_t originY);
void drawHomeBitrate(const String &bitrateText, int16_t originX,
                     int16_t originY);

void drawHomeTextField(const String &previousText, const String &text,
                       int16_t x, int16_t y, uint8_t datum, uint8_t font,
                       uint16_t color) {
  renderTarget->setTextDatum(datum);
  if (!previousText.isEmpty()) {
    renderTarget->setTextColor(backgroundColor, backgroundColor);
    renderTarget->drawString(previousText, x, y, font);
  }
  renderTarget->setTextColor(color, backgroundColor);
  renderTarget->drawString(text, x, y, font);
}

// Draw the home-screen radio controls, stream metadata, clock, and timer entry.
void drawHome() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("PET RADIO");
  drawHomeTop(0, 0);
  drawHomeMetadata(0, 0);
#if defined(BOARD_JC3248W535)
  drawButton(UI_LEFT + HOME_PREVIOUS_X, HOME_PLAYBACK_Y, HOME_PREVIOUS_WIDTH,
             HOME_PLAYBACK_HEIGHT, "|<", PANEL_LIGHT, TFT_WHITE, 2);
  drawButton(UI_LEFT + HOME_PLAY_X, HOME_PLAYBACK_Y, HOME_PLAY_WIDTH,
             HOME_PLAYBACK_HEIGHT, streamActive ? "PAUSE" : "PLAY",
             PANEL_LIGHT, TFT_WHITE, 2);
  drawButton(UI_LEFT + HOME_NEXT_X, HOME_PLAYBACK_Y, HOME_NEXT_WIDTH,
             HOME_PLAYBACK_HEIGHT, ">|", PANEL_LIGHT, TFT_WHITE, 2);

  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString("VOLUME", UI_LEFT, HOME_VOLUME_LABEL_Y, 2);
  display.setTextDatum(TR_DATUM);
  display.drawString(String(volume * 100 / 21) + "%",
                     UI_LEFT + UI_WIDTH, HOME_VOLUME_LABEL_Y, 2);
  display.fillRoundRect(UI_LEFT, HOME_VOLUME_SLIDER_Y, UI_WIDTH, 12, 6,
                        BUTTON_COLORS[buttonColorIndex]);
  const int16_t sliderWidth =
      static_cast<int16_t>((UI_WIDTH - 8) * volume / 21);
  display.fillRoundRect(UI_LEFT + 4, HOME_VOLUME_SLIDER_Y + 3, sliderWidth, 6,
                        3, ACCENT);
  display.fillCircle(UI_LEFT + 4 + sliderWidth, HOME_VOLUME_SLIDER_Y + 6, 9,
                     TFT_WHITE);
#else
  drawButton(UI_LEFT, 105, 49, 36, "|<", PANEL_LIGHT);
  drawButton(UI_LEFT + 55, 105, 100, 36,
             streamActive ? "PAUSE" : "PLAY", PANEL_LIGHT);
  drawButton(UI_LEFT + 161, 105, 49, 36, ">|", PANEL_LIGHT);

  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString("VOLUME", UI_LEFT, 151, 1);
  display.setTextDatum(TR_DATUM);
  display.drawString(String(volume * 100 / 21) + "%", UI_LEFT + UI_WIDTH,
                     151, 1);
  display.fillRoundRect(UI_LEFT, 168, UI_WIDTH, 8, 4,
                        BUTTON_COLORS[buttonColorIndex]);
  const int16_t sliderWidth =
      static_cast<int16_t>((UI_WIDTH - 4) * volume / 21);
  display.fillRoundRect(UI_LEFT + 2, 170, sliderWidth, 4, 2, ACCENT);
  display.fillCircle(UI_LEFT + 2 + sliderWidth, 172, 6, TFT_WHITE);
#endif

  drawButton(UI_LEFT, HOME_BOTTOM_BUTTON_Y, HOME_BOTTOM_BUTTON_WIDTH,
             HOME_BOTTOM_BUTTON_HEIGHT, "TIMER", PANEL_LIGHT);
  drawButton(UI_LEFT + HOME_BOTTOM_BUTTON_WIDTH + HOME_BOTTOM_BUTTON_GAP,
             HOME_BOTTOM_BUTTON_Y, HOME_BOTTOM_BUTTON_WIDTH,
             HOME_BOTTOM_BUTTON_HEIGHT, "MENU", PANEL_LIGHT);
}

void drawHomeTop(int16_t originX, int16_t originY) {
  TFT_eSPI &display = *renderTarget;
  struct tm localTime {};
  char clockText[24] = "--.--.---- --:--";
  if (getLocalTime(&localTime, 0)) {
    snprintf(clockText, sizeof(clockText), "%02d.%02d.%04d %02d:%02d",
             localTime.tm_mday, localTime.tm_mon + 1,
             localTime.tm_year + 1900, localTime.tm_hour, localTime.tm_min);
  }
  display.setTextDatum(TR_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
#if defined(BOARD_JC3248W535)
  display.drawString(clockText, UI_LEFT + UI_WIDTH, 4, 2);
#else
  display.drawString(clockText, UI_LEFT + UI_WIDTH - originX,
                     2 - originY, 1);
#endif
}

void drawHomeMetadata(int16_t originX, int16_t originY) {
  TFT_eSPI &display = *renderTarget;
  const int16_t left = UI_LEFT - originX;
#if defined(BOARD_JC3248W535)
  const int16_t top = 40 - originY;
#else
  const int16_t top = 36 - originY;
#endif
  const bool connected = WiFi.status() == WL_CONNECTED;
  const int codecId = connected && streamActive ? radioCodec() : 0;
  const uint32_t bitrateKbps =
      connected && streamActive ? (radioBitRate() + 500) / 1000 : 0;
  const char *codecName = "";
  if (codecId == 9) {
    codecName = "AAC";
  } else if (codecId > 0 && codecId < 9) {
    codecName = radioCodecName();
  }

  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  const String stationName =
      stationCount == 0 ? "No stations" : stations[selectedStation].name;
  const String fittedStationName =
      fittedText(stationName, UI_WIDTH - 14, 2);
#if defined(BOARD_JC3248W535)
  drawHomeTextField(displayedHomeStation, fittedStationName, left + 8,
                    top + 4, TL_DATUM, 2, primaryTextColor());
#else
  drawHomeTextField(displayedHomeStation, fittedStationName, left + 7,
                    top + 3, TL_DATUM, 2, primaryTextColor());
#endif
  displayedHomeStation = fittedStationName;
  String subtitle = streamTitle.isEmpty()
                        ? (streamStation.isEmpty() ? statusMessage
                                                   : streamStation)
                        : streamTitle;
  const TwoLineText titleLines =
      wrappedText(subtitle, UI_WIDTH - 14, 2);
#if defined(BOARD_JC3248W535)
  drawHomeTextField(displayedHomeTitleFirst, titleLines.first, left + 8,
                    top + 29, TL_DATUM, 2, TEXT_MUTED);
  displayedHomeTitleFirst = titleLines.first;
  drawHomeTextField(displayedHomeTitleSecond, titleLines.second, left + 8,
                    top + 51, TL_DATUM, 2, TEXT_MUTED);
  displayedHomeTitleSecond = titleLines.second;
  drawHomeTextField(displayedHomeCodec, codecName, left + 8, top + 100,
                    TL_DATUM, 1, TEXT_MUTED);
#else
  drawHomeTextField(displayedHomeTitleFirst, titleLines.first, left + 7,
                    top + 22, TL_DATUM, 2, TEXT_MUTED);
  displayedHomeTitleFirst = titleLines.first;
  drawHomeTextField(displayedHomeTitleSecond, titleLines.second, left + 7,
                    top + 38, TL_DATUM, 2, TEXT_MUTED);
  displayedHomeTitleSecond = titleLines.second;
  drawHomeTextField(displayedHomeCodec, codecName, left + 7, top + 55,
                    TL_DATUM, 1, TEXT_MUTED);
#endif
  displayedHomeCodec = codecName;
  const String bitrateText =
      bitrateKbps == 0 ? "-- kbit/s" : String(bitrateKbps) + " kbit/s";
  drawHomeBitrate(bitrateText, originX, originY);
}

void drawHomeBitrate(const String &bitrateText, int16_t originX,
                     int16_t originY) {
#if defined(BOARD_JC3248W535)
  drawHomeTextField(displayedHomeBitrate, bitrateText,
                    UI_LEFT + UI_WIDTH - 8 - originX, 140 - originY,
                    TR_DATUM, 1, TEXT_MUTED);
#else
  drawHomeTextField(displayedHomeBitrate, bitrateText,
                    UI_LEFT + UI_WIDTH - 7 - originX, 91 - originY,
                    TR_DATUM, 1, TEXT_MUTED);
#endif
  displayedHomeBitrate = bitrateText;
}

void refreshHomeBitrate() {
  const bool connected = WiFi.status() == WL_CONNECTED;
  const uint32_t bitrateKbps =
      connected && streamActive ? (radioBitRate() + 500) / 1000 : 0;
  const String bitrateText =
      bitrateKbps == 0 ? "-- kbit/s" : String(bitrateKbps) + " kbit/s";
  renderTarget = &display;
  drawHomeBitrate(bitrateText, 0, 0);
  homeBitrateDirty = false;
  renderTarget = &display;
}

void refreshHomeMetadata() {
  renderTarget = &display;
  drawHomeMetadata(0, 0);
  homeMetadataDirty = false;
  homeBitrateDirty = false;
  renderTarget = &display;
}

void refreshHomeClock() {
  constexpr int16_t CLOCK_REFRESH_LEFT = 96;
  constexpr int16_t CLOCK_REFRESH_WIDTH = HOME_UI_WIDTH - CLOCK_REFRESH_LEFT;
  if (homeUpdateFramebufferReady) {
    renderTarget = &homeUpdateFrameBuffer;
    homeUpdateFrameBuffer.fillSprite(backgroundColor);
    drawHomeTop(UI_LEFT, 0);
    homeUpdateFrameBuffer.drawFastHLine(CLOCK_REFRESH_LEFT, 31,
                                        CLOCK_REFRESH_WIDTH, 0x4208);
    homeUpdateFrameBuffer.pushSprite(UI_LEFT, 0, CLOCK_REFRESH_LEFT, 0,
                                     CLOCK_REFRESH_WIDTH, 32);
  } else {
    renderTarget = &display;
    display.fillRect(UI_LEFT + CLOCK_REFRESH_LEFT, 0, CLOCK_REFRESH_WIDTH, 32,
                     backgroundColor);
    drawHomeTop(0, 0);
    display.drawFastHLine(UI_LEFT + CLOCK_REFRESH_LEFT, 31,
                          CLOCK_REFRESH_WIDTH, 0x4208);
  }
  homeClockDirty = false;
  renderTarget = &display;
}

// Route touches on the timer panel to duration, start, pause, or confirm actions.
void handleTimerTouch(int16_t x, int16_t y, uint32_t now) {
  if ((timerState == TimerState::Ready || timerState == TimerState::Paused) &&
      pointInRect(x, y, 4, 39, 43, 25)) {
    changeTimerDuration(60);
  } else if ((timerState == TimerState::Ready ||
              timerState == TimerState::Paused) &&
             pointInRect(x, y, 49, 39, 43, 25)) {
    changeTimerDuration(10);
  } else if ((timerState == TimerState::Ready ||
              timerState == TimerState::Paused) &&
             pointInRect(x, y, 4, 123, 43, 25)) {
    changeTimerDuration(-60);
  } else if ((timerState == TimerState::Ready ||
              timerState == TimerState::Paused) &&
             pointInRect(x, y, 49, 123, 43, 25)) {
    changeTimerDuration(-10);
  } else if (pointInRect(x, y, 8, 158, 80, 27)) {
    if (timerState == TimerState::Running ||
        timerState == TimerState::Expired) {
      stopOrConfirmTimer(now);
    } else {
      startTimer(now);
    }
  }
}

// Draw the settings category menu.
void drawSettings() {
  drawPanelHeader("SETTINGS");
  drawMenuBackButton();
  const int16_t rowStep = SETTINGS_ITEM_HEIGHT + SETTINGS_ITEM_GAP;
  const char *labels[] = {"AUDIO", "DISPLAY & TOUCH", "WI-FI", "STATIONS"};
  for (uint8_t row = 0; row < 4; ++row) {
    drawButton(UI_LEFT, SETTINGS_ITEM_START_Y + row * rowStep, UI_WIDTH,
               SETTINGS_ITEM_HEIGHT, labels[row], PANEL_LIGHT, TFT_WHITE, 2,
               false);
  }
#if defined(BOARD_FREENOVE_FNK0114L)
  drawButton(UI_LEFT, SETTINGS_ITEM_START_Y + 4 * rowStep, UI_WIDTH,
             SETTINGS_ITEM_HEIGHT, "RGB LED", PANEL_LIGHT, TFT_WHITE, 2,
             false);
#endif
}

// Draw audio options and a touch-friendly volume control.
void drawAudioSettings() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("AUDIO");
  drawMenuBackButton();
#if defined(BOARD_JC3248W535)
  const String labels[] = {
      String("AUDIO BOOST +6 dB: ") + (audioBoost ? "ON" : "OFF"),
      String("SPECTRUM: ") + (audioSpectrumEnabled ? "ON" : "OFF"),
      String("DYNAMIC COMPRESSION: ") +
          (audioCompressorEnabled ? "ON" : "OFF")};
  constexpr uint8_t TOGGLE_COUNT = 3;
#else
  const String labels[] = {
      String("AUDIO BOOST +6 dB: ") + (audioBoost ? "ON" : "OFF"),
      String("SPECTRUM: ") + (audioSpectrumEnabled ? "ON" : "OFF"),
      String("DYNAMIC COMPRESSION: ") +
          (audioCompressorEnabled ? "ON" : "OFF"),
      String("GPIO4 AMP: ") + (audioAmpEnabled ? "ON" : "OFF"),
      String("NOISE SHAPING: ") + (audioNoiseShaping ? "ON" : "OFF")};
  constexpr uint8_t TOGGLE_COUNT = 5;
#endif
  for (uint8_t row = 0; row < TOGGLE_COUNT; ++row) {
    drawButton(UI_LEFT, AUDIO_TOGGLE_START_Y + row * AUDIO_TOGGLE_STEP,
               UI_WIDTH, AUDIO_TOGGLE_HEIGHT, labels[row], PANEL_LIGHT,
               TFT_WHITE, 2, false);
  }
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, backgroundColor);
  display.drawString("VOLUME", UI_LEFT, AUDIO_VOLUME_LABEL_Y, 2);
  display.setTextDatum(TR_DATUM);
  display.drawString(String(volume * 100 / 21) + "%", UI_LEFT + UI_WIDTH,
                     AUDIO_VOLUME_LABEL_Y, 2);
  display.fillRoundRect(UI_LEFT, AUDIO_VOLUME_TRACK_Y, UI_WIDTH, 10, 5,
                        BUTTON_COLORS[buttonColorIndex]);
  const int16_t sliderWidth =
      static_cast<int16_t>((UI_WIDTH - 4) * volume / 21);
  display.fillRoundRect(UI_LEFT + 2, AUDIO_VOLUME_TRACK_Y + 2, sliderWidth, 6, 3,
                        ACCENT);
  display.fillCircle(UI_LEFT + 2 + sliderWidth, AUDIO_VOLUME_TRACK_Y + 5, 8,
                     TFT_WHITE);
}

// Draw larger color targets and a dedicated touchscreen calibration action.
void drawDisplaySettings() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("DISPLAY & TOUCH");
  drawMenuBackButton();
  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
#if defined(BOARD_JC3248W535)
  display.drawString("BACKGROUND", UI_LEFT, 38, 2);
  constexpr int16_t BACKGROUND_SWATCH_Y = 62;
  constexpr int16_t BUTTON_SWATCH_Y = 162;
  constexpr int16_t SWATCH_HEIGHT = 28;
  constexpr int16_t SWATCH_ROW_STEP = 34;
#else
  display.drawString("BACKGROUND", UI_LEFT, 35, 2);
  constexpr int16_t BACKGROUND_SWATCH_Y = 53;
  constexpr int16_t BUTTON_SWATCH_Y = 128;
  constexpr int16_t SWATCH_HEIGHT = 24;
  constexpr int16_t SWATCH_ROW_STEP = 29;
#endif
  for (uint8_t index = 0; index < THEME_COLOR_COUNT; ++index) {
    const int16_t swatchX = themeSwatchX(index % COLORS_PER_ROW);
    const int16_t swatchY =
        BACKGROUND_SWATCH_Y + (index / COLORS_PER_ROW) * SWATCH_ROW_STEP;
    display.fillRoundRect(swatchX, swatchY, themeSwatchWidth, SWATCH_HEIGHT, 4,
                          THEME_COLORS[index]);
    display.drawRoundRect(
        swatchX - 1, swatchY - 1, themeSwatchWidth + 2, SWATCH_HEIGHT + 2, 5,
        index == themeColorIndex ? primaryTextColor() : 0x4208);
  }
#if defined(BOARD_JC3248W535)
  display.drawString("BUTTONS", UI_LEFT, 138, 2);
#else
  display.drawString("BUTTONS", UI_LEFT, 110, 2);
#endif
  for (uint8_t index = 0; index < BUTTON_COLOR_COUNT; ++index) {
    const int16_t swatchX = themeSwatchX(index % COLORS_PER_ROW);
    const int16_t swatchY =
        BUTTON_SWATCH_Y + (index / COLORS_PER_ROW) * SWATCH_ROW_STEP;
    display.fillRoundRect(swatchX, swatchY, themeSwatchWidth, SWATCH_HEIGHT, 4,
                          BUTTON_COLORS[index]);
    display.drawRoundRect(
        swatchX - 1, swatchY - 1, themeSwatchWidth + 2, SWATCH_HEIGHT + 2, 5,
        index == buttonColorIndex ? primaryTextColor() : 0x4208);
  }
#if defined(BOARD_JC3248W535)
  display.drawString("BRIGHTNESS", UI_LEFT, 228, 2);
  display.setTextDatum(TR_DATUM);
  display.drawString(String(displayBrightness) + "%", UI_LEFT + UI_WIDTH,
                     228, 2);
  display.fillRoundRect(UI_LEFT, 247, UI_WIDTH, 10, 5,
                        BUTTON_COLORS[buttonColorIndex]);
  const int16_t brightnessWidth = static_cast<int16_t>(
      (UI_WIDTH - 4) * (displayBrightness - DISPLAY_BRIGHTNESS_MIN) /
      (DISPLAY_BRIGHTNESS_MAX - DISPLAY_BRIGHTNESS_MIN));
  display.fillRoundRect(UI_LEFT + 2, 249, brightnessWidth, 6, 3, ACCENT);
  display.fillCircle(UI_LEFT + 2 + brightnessWidth, 252, 8, TFT_WHITE);
  drawButton(UI_LEFT, 270, UI_WIDTH, 38, "RECALIBRATE TOUCHSCREEN",
             PANEL_LIGHT, TFT_WHITE, 2, false);
#else
  drawButton(UI_LEFT, 194, UI_WIDTH, 36, "RECALIBRATE TOUCHSCREEN",
             PANEL_LIGHT, TFT_WHITE, 2, false);
#endif
}

#if defined(BOARD_FREENOVE_FNK0114L)
// Draw controls available only on the FNK0114L's rear RGB LED.
void drawRgbLedSettings() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("RGB LED OPTIONS");
  drawMenuBackButton();
  drawButton(UI_LEFT, 48, UI_WIDTH, 32,
             String("LED: ") + (rgbLedEnabled ? "ON" : "OFF"),
             PANEL_LIGHT, TFT_WHITE, 2);
  drawButton(UI_LEFT, 88, UI_WIDTH, 32,
             String("SLOW PULSE: ") + (rgbLedPulseEnabled ? "ON" : "OFF"),
             PANEL_LIGHT, TFT_WHITE, 2);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString("COLOR", UI_LEFT, 132, 1);

  constexpr int16_t COLOR_BUTTON_WIDTH = 52;
  constexpr int16_t COLOR_BUTTON_GAP = 7;
  constexpr int16_t COLOR_BUTTON_HEIGHT = 38;
  constexpr int16_t COLOR_BUTTON_Y = 150;
  const int16_t colorsWidth =
      RGB_LED_COLOR_COUNT * COLOR_BUTTON_WIDTH +
      (RGB_LED_COLOR_COUNT - 1) * COLOR_BUTTON_GAP;
  const int16_t colorsLeft = UI_LEFT + (UI_WIDTH - colorsWidth) / 2;
  const char *colorLabels[RGB_LED_COLOR_COUNT] = {
      "RED", "CYAN", "VIOLET", "AMBER", "GREEN"};
  for (uint8_t index = 0; index < RGB_LED_COLOR_COUNT; ++index) {
    const int16_t x =
        colorsLeft + index * (COLOR_BUTTON_WIDTH + COLOR_BUTTON_GAP);
    const RgbColor color = RGB_LED_COLORS[index];
    const uint16_t color565 =
        display.color565(color.red, color.green, color.blue);
    display.fillRoundRect(x, COLOR_BUTTON_Y, COLOR_BUTTON_WIDTH,
                          COLOR_BUTTON_HEIGHT, 5, color565);
    display.drawRoundRect(
        x - 1, COLOR_BUTTON_Y - 1, COLOR_BUTTON_WIDTH + 2,
        COLOR_BUTTON_HEIGHT + 2, 6,
        index == rgbLedColorIndex ? TFT_WHITE : 0x4208);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(TFT_WHITE, color565);
    display.drawString(colorLabels[index], x + COLOR_BUTTON_WIDTH / 2,
                       COLOR_BUTTON_Y + COLOR_BUTTON_HEIGHT / 2, 1);
  }
}

// Set the FNK0114L RGB LED brightness while keeping its selected color.
void setRgbLedOutput(uint8_t brightness) {
  const RgbColor color = RGB_LED_COLORS[rgbLedColorIndex];
  const uint32_t red = color.red * brightness / 255;
  const uint32_t green = color.green * brightness / 255;
  const uint32_t blue = color.blue * brightness / 255;
  ledcWrite(RGB_LED_RED_CHANNEL, 255 - red);
  ledcWrite(RGB_LED_GREEN_CHANNEL, 255 - green);
  ledcWrite(RGB_LED_BLUE_CHANNEL, 255 - blue);
}

// Initialize the board-specific LED output and start it in the off state.
void initializeRgbLed() {
  ledcSetup(RGB_LED_RED_CHANNEL, 5000, 8);
  ledcSetup(RGB_LED_GREEN_CHANNEL, 5000, 8);
  ledcSetup(RGB_LED_BLUE_CHANNEL, 5000, 8);
  ledcAttachPin(RGB_LED_RED_PIN, RGB_LED_RED_CHANNEL);
  ledcAttachPin(RGB_LED_GREEN_PIN, RGB_LED_GREEN_CHANNEL);
  ledcAttachPin(RGB_LED_BLUE_PIN, RGB_LED_BLUE_CHANNEL);
  setRgbLedOutput(0);
}

// Update timer-dependent LED pulsing and the optional normal pulse mode.
void updateRgbLed(uint32_t now) {
  if (now - lastRgbLedUpdateAt < RGB_LED_UPDATE_INTERVAL_MS) {
    return;
  }
  lastRgbLedUpdateAt = now;

  if (timerModeActive()) {
    if (!rgbLedEnabled) {
      setRgbLedOutput(0);
      return;
    }
    const uint32_t totalMs = max<uint32_t>(timerDurationSeconds * 1000, 1);
    const uint32_t remainingMs = timerRemainingMs(now);
    const uint32_t periodMs =
        500 + 2500 * min<uint32_t>(remainingMs, totalMs) / totalMs;
    const uint32_t phase = now % periodMs;
    const uint32_t halfPeriod = periodMs / 2;
    const uint32_t distance =
        phase < halfPeriod ? phase : periodMs - phase;
    const uint32_t ramp = distance * 255 / halfPeriod;
    const uint8_t brightness =
        static_cast<uint8_t>(20 + (ramp * ramp * 140) / (255 * 255));
    setRgbLedOutput(brightness);
    return;
  }

  if (!rgbLedEnabled) {
    setRgbLedOutput(0);
    return;
  }
  if (!rgbLedPulseEnabled) {
    setRgbLedOutput(72);
    return;
  }

  const uint32_t phase = now % RGB_LED_PULSE_PERIOD_MS;
  const uint32_t halfPeriod = RGB_LED_PULSE_PERIOD_MS / 2;
  const uint32_t distance = phase < halfPeriod ? phase : RGB_LED_PULSE_PERIOD_MS - phase;
  const uint32_t ramp = distance * 255 / halfPeriod;
  const uint8_t brightness =
      static_cast<uint8_t>(16 + (ramp * ramp * 104) / (255 * 255));
  setRgbLedOutput(brightness);
}

void setRgbLedEnabled(bool enabled) {
  rgbLedEnabled = enabled;
  if (preferences.putBool("rgbEnabled", rgbLedEnabled) == 0) {
    Serial.println("ERROR: could not save RGB LED setting.");
    setStatus("Could not save RGB LED setting");
    return;
  }
  markUiDirty();
}

void setRgbLedPulseEnabled(bool enabled) {
  rgbLedPulseEnabled = enabled;
  if (preferences.putBool("rgbPulse", rgbLedPulseEnabled) == 0) {
    Serial.println("ERROR: could not save RGB LED pulse setting.");
    setStatus("Could not save RGB LED pulse setting");
    return;
  }
  markUiDirty();
}

void setRgbLedColor(uint8_t index) {
  if (index >= RGB_LED_COLOR_COUNT || index == rgbLedColorIndex) {
    return;
  }
  rgbLedColorIndex = index;
  if (preferences.putUChar("rgbColor", rgbLedColorIndex) == 0) {
    Serial.println("ERROR: could not save RGB LED color.");
    setStatus("Could not save RGB LED color");
    return;
  }
  markUiDirty();
}
#endif

// Draw the saved Wi-Fi credentials and connection status controls.
void drawWifiSettings() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("WI-FI SETTINGS");
  drawMenuBackButton();
  const int16_t labelLeft = UI_LEFT + 6;
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, backgroundColor);
#if defined(BOARD_JC3248W535)
  constexpr int16_t SSID_LABEL_Y = 40;
  constexpr int16_t SSID_FIELD_Y = 62;
  constexpr int16_t FIELD_HEIGHT = 48;
  constexpr int16_t PASSWORD_LABEL_Y = 124;
  constexpr int16_t PASSWORD_FIELD_Y = 146;
  constexpr int16_t SHOW_PASSWORD_Y = 202;
  constexpr int16_t CONNECT_Y = 254;
  constexpr int16_t STATUS_Y = 308;
#else
  constexpr int16_t SSID_LABEL_Y = 37;
  constexpr int16_t SSID_FIELD_Y = 55;
  constexpr int16_t FIELD_HEIGHT = 39;
  constexpr int16_t PASSWORD_LABEL_Y = 100;
  constexpr int16_t PASSWORD_FIELD_Y = 118;
  constexpr int16_t SHOW_PASSWORD_Y = 157;
  constexpr int16_t CONNECT_Y = 198;
  constexpr int16_t STATUS_Y = 231;
#endif
  display.drawString("NETWORK NAME (SSID)", labelLeft, SSID_LABEL_Y, 2);
  drawButton(UI_LEFT, SSID_FIELD_Y, UI_WIDTH, FIELD_HEIGHT,
             fittedText(wifiSsid.isEmpty() ? "(Enter SSID)" : wifiSsid,
                        UI_WIDTH - 16, 2),
             PANEL, TFT_WHITE, 2, false);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, backgroundColor);
  display.drawString("PASSWORD", labelLeft, PASSWORD_LABEL_Y, 2);
  String passwordPreview = passwordForDisplay(wifiPassword);
  if (wifiPassword.isEmpty()) {
    passwordPreview = "(Enter password)";
  }
  drawButton(UI_LEFT, PASSWORD_FIELD_Y, UI_WIDTH, FIELD_HEIGHT,
             fittedText(passwordPreview, UI_WIDTH - 16, 2), PANEL, TFT_WHITE,
             2, false);

#if defined(BOARD_JC3248W535)
  drawButton(UI_LEFT, SHOW_PASSWORD_Y, UI_WIDTH, 44,
#else
  drawButton(UI_LEFT, SHOW_PASSWORD_Y, UI_WIDTH, 34,
#endif
             String("SHOW PASSWORD: ") + (showWifiPassword ? "ON" : "OFF"),
             showWifiPassword ? 0x2147 : PANEL, TFT_WHITE, 2, false);
#if defined(BOARD_JC3248W535)
  drawButton(UI_LEFT, CONNECT_Y, UI_WIDTH, 44, "CONNECT",
#else
  drawButton(UI_LEFT, CONNECT_Y, UI_WIDTH, 32, "CONNECT",
#endif
             wifiConnecting ? 0x3186 : ACCENT, TFT_WHITE, 2, false);
  drawText(fittedText(wifiStatusText(), UI_WIDTH, 1), UI_LEFT, STATUS_Y, 1,
           WiFi.status() == WL_CONNECTED ? SUCCESS : TEXT_MUTED);
}

// Draw the paged station list and its inline delete confirmation.
void drawStations() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader("STATION LIST");
  drawMenuBackButton();
  const uint8_t pageCount = stationPageCount();
  const uint8_t pageStart = stationListPage * STATIONS_PER_PAGE;
  const uint8_t visibleStations =
      min<uint8_t>(STATIONS_PER_PAGE, stationCount - pageStart);
#if defined(BOARD_JC3248W535)
  constexpr int16_t ROW_START_Y = 50;
  constexpr int16_t ROW_STEP = 49;
  constexpr int16_t ROW_HEIGHT = 43;
  constexpr int16_t NAME_WIDTH = 260;
  constexpr int16_t EDIT_X = 268;
  constexpr int16_t EDIT_WIDTH = 80;
  constexpr int16_t DELETE_X = 356;
  constexpr int16_t DELETE_WIDTH = 100;
  constexpr int16_t PAGER_Y = 263;
  constexpr int16_t PAGER_HEIGHT = 42;
  drawText(String(stationCount) + " STATIONS", UI_LEFT, 35, 1, TEXT_MUTED);
  if (deleteConfirmation) {
    display.fillRoundRect(UI_LEFT + 24, 100, UI_WIDTH - 48, 148, 10,
                          PANEL_LIGHT);
    display.drawRoundRect(UI_LEFT + 24, 100, UI_WIDTH - 48, 148, 10,
                          TFT_CYAN);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(primaryTextColor(), PANEL_LIGHT);
    display.drawString("Delete station?", DISPLAY_WIDTH / 2, 122, 2);
    display.drawString(fittedText(stations[deleteIndex].name, UI_WIDTH - 72, 2),
                       DISPLAY_WIDTH / 2, 152, 2);
    const int16_t actionWidth = (UI_WIDTH - 32) / 2;
    drawButton(UI_LEFT + 8, 184, actionWidth, 48, "CANCEL", PANEL,
               TFT_WHITE, 2, false);
    drawButton(UI_LEFT + 24 + actionWidth, 184, actionWidth, 48, "DELETE",
               ERROR_COLOUR, TFT_WHITE, 2, false);
    return;
  }

  for (uint8_t row = 0; row < visibleStations; ++row) {
    const uint8_t index = pageStart + row;
    const int16_t rowY = ROW_START_Y + row * ROW_STEP;
    const uint16_t fill = index == selectedStation ? 0x2147 : PANEL;
    drawButton(UI_LEFT, rowY, NAME_WIDTH, ROW_HEIGHT,
               fittedText(stations[index].name, NAME_WIDTH - 12, 2), fill,
               TFT_WHITE, 2, false);
    drawButton(UI_LEFT + EDIT_X, rowY, EDIT_WIDTH, ROW_HEIGHT, "EDIT",
               PANEL_LIGHT, TFT_WHITE, 2, false);
    drawButton(UI_LEFT + DELETE_X, rowY, DELETE_WIDTH, ROW_HEIGHT, "DELETE",
               ERROR_COLOUR, TFT_WHITE, 2, false);
  }
  drawButton(UI_LEFT, PAGER_Y, 60, PAGER_HEIGHT, "<", PANEL_LIGHT,
             TFT_WHITE, 2, false);
  drawButton(UI_LEFT + 70, PAGER_Y, 60, PAGER_HEIGHT, ">", PANEL_LIGHT,
             TFT_WHITE, 2, false);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString(String(stationListPage + 1) + "/" + String(pageCount),
                     UI_LEFT + 225, PAGER_Y + PAGER_HEIGHT / 2, 2);
  drawButton(UI_LEFT + 300, PAGER_Y, UI_WIDTH - 300, PAGER_HEIGHT, "+ ADD",
             ACCENT, TFT_WHITE, 2, false);
#else
  drawText(String(stationCount) + " STATIONS", UI_LEFT, 34, 1, TEXT_MUTED);
  if (deleteConfirmation) {
    for (uint8_t row = 0; row < visibleStations; ++row) {
      const uint8_t i = pageStart + row;
      const int16_t rowY = 44 + row * 36;
      const uint16_t fill = i == selectedStation ? 0x2147 : PANEL;
      drawButton(UI_LEFT, rowY, 178, 32,
                 fittedText(stations[i].name, 168, 2), fill, TFT_WHITE, 2,
                 false);
      drawButton(UI_LEFT + 184, rowY, 52, 32, "EDIT", PANEL_LIGHT,
                 TFT_WHITE, 2, false);
      drawButton(UI_LEFT + 242, rowY, 54, 32, "DEL", ERROR_COLOUR,
                 TFT_WHITE, 2, false);
    }
    display.fillRoundRect(UI_LEFT + 9, 78, UI_WIDTH - 18, 92, 8, PANEL_LIGHT);
    display.drawRoundRect(UI_LEFT + 9, 78, UI_WIDTH - 18, 92, 8, TFT_CYAN);
    display.setTextDatum(MC_DATUM);
    display.setTextColor(primaryTextColor(), PANEL_LIGHT);
    display.drawString("Delete station?", 160, 98, 2);
    display.drawString(
        fittedText(stations[deleteIndex].name, 240, 2), 160, 121, 2);
    drawButton(UI_LEFT + 14, 136, 127, 28, "CANCEL", PANEL, TFT_WHITE, 2,
               false);
    drawButton(UI_LEFT + 147, 136, 127, 28, "DELETE", ERROR_COLOUR,
               TFT_WHITE, 2, false);
    return;
  }

  for (uint8_t row = 0; row < visibleStations; ++row) {
    const uint8_t i = pageStart + row;
    const int16_t rowY = 44 + row * 36;
    const uint16_t fill = i == selectedStation ? 0x2147 : PANEL;
    drawButton(UI_LEFT, rowY, 178, 32,
               fittedText(stations[i].name, 168, 2), fill, TFT_WHITE, 2,
               false);
    drawButton(UI_LEFT + 184, rowY, 52, 32, "EDIT", PANEL_LIGHT,
               TFT_WHITE, 2, false);
    drawButton(UI_LEFT + 242, rowY, 54, 32, "DEL", ERROR_COLOUR,
               TFT_WHITE, 2, false);
  }
  drawButton(UI_LEFT, 197, 40, 36, "<", PANEL_LIGHT, TFT_WHITE, 2, false);
  drawButton(UI_LEFT + 46, 197, 40, 36, ">", PANEL_LIGHT, TFT_WHITE, 2,
             false);
  display.setTextDatum(MC_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString(String(stationListPage + 1) + "/" + String(pageCount),
                     UI_LEFT + 125, 215, 2);
  drawButton(UI_LEFT + 164, 197, 132, 36, "+ ADD", ACCENT,
             TFT_WHITE, 2, false);
#endif
}

// Draw editable station fields and save/back actions.
void drawStationEditor() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader(stationEditorIsNew ? "NEW STATION" : "EDIT STATION");
  display.setTextDatum(TL_DATUM);
  display.setTextColor(TEXT_MUTED, backgroundColor);
#if defined(BOARD_JC3248W535)
  constexpr int16_t NAME_LABEL_Y = 42;
  constexpr int16_t NAME_FIELD_Y = 64;
  constexpr int16_t NAME_FIELD_HEIGHT = 48;
  constexpr int16_t URL_LABEL_Y = 126;
  constexpr int16_t URL_FIELD_Y = 148;
  constexpr int16_t URL_FIELD_HEIGHT = 48;
  constexpr int16_t ACTION_Y = 220;
  constexpr int16_t ACTION_HEIGHT = 48;
  constexpr int16_t HINT_Y = 280;
#else
  constexpr int16_t NAME_LABEL_Y = 39;
  constexpr int16_t NAME_FIELD_Y = 57;
  constexpr int16_t NAME_FIELD_HEIGHT = 39;
  constexpr int16_t URL_LABEL_Y = 104;
  constexpr int16_t URL_FIELD_Y = 122;
  constexpr int16_t URL_FIELD_HEIGHT = 39;
  constexpr int16_t ACTION_Y = 181;
  constexpr int16_t ACTION_HEIGHT = 40;
  constexpr int16_t HINT_Y = 225;
#endif
  display.drawString("NAME", UI_LEFT, NAME_LABEL_Y, 2);
  drawButton(UI_LEFT, NAME_FIELD_Y, UI_WIDTH, NAME_FIELD_HEIGHT,
             fittedText(editName, UI_WIDTH - 14, 2), PANEL, TFT_WHITE, 2,
             false);
  display.setTextColor(TEXT_MUTED, backgroundColor);
  display.drawString("STREAM URL", UI_LEFT, URL_LABEL_Y, 2);
  drawButton(UI_LEFT, URL_FIELD_Y, UI_WIDTH, URL_FIELD_HEIGHT,
             fittedText(editUrl, UI_WIDTH - 14, 2), PANEL, TFT_WHITE, 2,
             false);
  const int16_t actionWidth = (UI_WIDTH - 8) / 2;
#if defined(BOARD_JC3248W535)
  drawButton(UI_LEFT, ACTION_Y, actionWidth, ACTION_HEIGHT, "SAVE", ACCENT,
             TFT_WHITE, 2, false);
#else
  drawButton(UI_LEFT, ACTION_Y, actionWidth, ACTION_HEIGHT, "SAVE", ACCENT,
             TFT_WHITE, 2, false);
#endif
  drawButton(UI_LEFT + actionWidth + 8, ACTION_Y, actionWidth, ACTION_HEIGHT,
             "BACK",
             PANEL_LIGHT, TFT_WHITE, 2, false);
  drawText("MP3/AAC stream (HTTP/HTTPS)", UI_LEFT, HINT_Y, 1, TEXT_MUTED);
}

// Draw the adaptive on-screen keyboard for credentials and station fields.
void drawKeyboard() {
  TFT_eSPI &display = *renderTarget;
  drawPanelHeader(keyboardTarget == KeyboardTarget::Ssid
                      ? "ENTER SSID"
                  : keyboardTarget == KeyboardTarget::Password
                      ? "ENTER PASSWORD"
                  : keyboardTarget == KeyboardTarget::StationName
                      ? "STATION NAME"
                      : "ENTER STREAM URL");
  const bool editingPassword = keyboardTarget == KeyboardTarget::Password;
  const String visibleValue =
      editingPassword ? passwordForDisplay(keyboardValue) : keyboardValue;
  const int16_t valueWidth = editingPassword && !showWifiPassword
                                 ? UI_WIDTH - (wideKeyboardLayout() ? 78 : 70)
                                 : UI_WIDTH;
  drawText(fittedText(visibleValue, valueWidth, 2), UI_LEFT,
           wideKeyboardLayout() ? 38 : 40, 2, TFT_WHITE);
  if (editingPassword) {
    const int16_t showWidth = wideKeyboardLayout() ? 70 : 62;
    drawButton(UI_LEFT + UI_WIDTH - showWidth, 32, showWidth, 30,
               showWifiPassword ? "HIDE" : "SHOW", PANEL_LIGHT,
               TFT_WHITE, 1, false);
  }

  for (uint8_t row = 0; row < 3; ++row) {
    const String keys = keyboardRow(row);
    if (keys.isEmpty()) {
      continue;
    }
    const int16_t keyGap = keyboardKeyGap();
    const int16_t keyWidth = keyboardKeyWidth(keys);
    const int16_t rowWidth =
        keys.length() * keyWidth + (keys.length() - 1) * keyGap;
    const int16_t rowLeft = UI_LEFT + (UI_WIDTH - rowWidth) / 2;
    for (uint8_t column = 0; column < keys.length(); ++column) {
      const int16_t x = rowLeft + column * (keyWidth + keyGap);
      const int16_t y = keyboardRowY(row);
      String label(keys[column]);
      if (keyboardPage == 0 && keyboardUppercase) {
        label.toUpperCase();
      }
      drawButton(x, y, keyWidth, keyboardKeyHeight(), label, PANEL_LIGHT,
                 TFT_WHITE, 2, false);
    }
  }

  const int16_t controlY = keyboardControlY();
  const int16_t controlHeight = keyboardControlHeight();
  int16_t controlX = 0;
  int16_t controlWidth = 0;
  keyboardControlBounds(0, controlX, controlWidth);
  drawButton(controlX, controlY, controlWidth, controlHeight,
             keyboardPage == 0 && !keyboardUppercase ? "abc" : "ABC",
             PANEL_LIGHT, TFT_WHITE, 2, false);
  keyboardControlBounds(1, controlX, controlWidth);
  drawButton(controlX, controlY, controlWidth, controlHeight,
             keyboardPage == 0 ? "?123"
                               : keyboardPage == 1 ? "SYM2" : "SYM1",
             PANEL_LIGHT, TFT_WHITE, 2, false);
  keyboardControlBounds(2, controlX, controlWidth);
  drawButton(controlX, controlY, controlWidth, controlHeight, "SPACE",
             PANEL_LIGHT, TFT_WHITE, 2, false);
  keyboardControlBounds(3, controlX, controlWidth);
  drawButton(controlX, controlY, controlWidth, controlHeight, "<-",
             PANEL_LIGHT, TFT_WHITE, 2, false);
  const int16_t actionY = keyboardActionY();
  const int16_t actionHeight = keyboardActionHeight();
  const int16_t actionWidth = wideKeyboardLayout() ? (UI_WIDTH - 8) / 2 : 98;
  constexpr int16_t actionGap = 8;
#if defined(BOARD_JC3248W535)
  constexpr uint8_t ACTION_FONT = 2;
#else
  constexpr uint8_t ACTION_FONT = 1;
#endif
  drawButton(UI_LEFT, actionY, actionWidth, actionHeight, "CANCEL",
             PANEL_LIGHT, TFT_WHITE, ACTION_FONT, false);
  drawButton(UI_LEFT + actionWidth + actionGap, actionY,
             wideKeyboardLayout() ? actionWidth : 104, actionHeight, "DONE",
             ACCENT, TFT_WHITE, ACTION_FONT, false);
}

// Render the current screen into the sprite and then refresh the display.
void renderScreen() {
  updateScreenLayout();
#if defined(BOARD_JC3248W535)
  display.setNativeCoordinates(true);
#endif
  renderTarget = displayFramebufferReady
                     ? static_cast<TFT_eSPI *>(&screenFrameBuffer)
                     : &display;
  if (displayFramebufferReady) {
    screenFrameBuffer.fillSprite(backgroundColor);
  } else if (currentScreen == Screen::Home && petPanelFramebufferReady) {
    display.fillRect(CAT_PANEL_WIDTH, 0, DISPLAY_WIDTH - CAT_PANEL_WIDTH,
                     DISPLAY_HEIGHT, backgroundColor);
  } else {
    display.fillScreen(backgroundColor);
  }
  if (currentScreen == Screen::Home) {
#if defined(BOARD_JC3248W535)
    renderTarget->setViewport(CAT_PANEL_WIDTH, 0,
                              jc3248w535::DISPLAY_WIDTH - CAT_PANEL_WIDTH,
                              jc3248w535::DISPLAY_HEIGHT, false);
#else
    renderTarget->setViewport(CAT_PANEL_WIDTH, 0,
                              DISPLAY_WIDTH - CAT_PANEL_WIDTH,
                              DISPLAY_HEIGHT, false);
#endif
  }
  switch (currentScreen) {
    case Screen::Home:
      drawHome();
      break;
    case Screen::Settings:
      drawSettings();
      break;
    case Screen::AudioSettings:
      drawAudioSettings();
      break;
    case Screen::DisplaySettings:
      drawDisplaySettings();
      break;
#if defined(BOARD_FREENOVE_FNK0114L)
    case Screen::RgbLedSettings:
      drawRgbLedSettings();
      break;
#endif
    case Screen::Wifi:
      drawWifiSettings();
      break;
    case Screen::Stations:
      drawStations();
      break;
    case Screen::StationEditor:
      drawStationEditor();
      break;
    case Screen::Keyboard:
      drawKeyboard();
      break;
  }
  if (currentScreen == Screen::Home) {
    renderTarget->resetViewport();
    drawCatFrame(millis(), true);
    homeMetadataDirty = false;
    homeBitrateDirty = false;
    homeClockDirty = false;
  } else if (displayFramebufferReady) {
    screenFrameBuffer.pushSprite(0, 0);
  }
  uiDirty = false;
  renderTarget = &display;
}

bool fitTouchCalibration(const uint16_t *rawX, const uint16_t *rawY,
                         TouchCalibration &fit) {
  double meanX = 0;
  double meanY = 0;
  for (uint8_t i = 0; i < TOUCH_CALIBRATION_TARGET_COUNT; ++i) {
    meanX += rawX[i];
    meanY += rawY[i];
  }
  meanX /= TOUCH_CALIBRATION_TARGET_COUNT;
  meanY /= TOUCH_CALIBRATION_TARGET_COUNT;

  double xx = 0;
  double xy = 0;
  double yy = 0;
  double xTargetX = 0;
  double yTargetX = 0;
  double xTargetY = 0;
  double yTargetY = 0;
  for (uint8_t i = 0; i < TOUCH_CALIBRATION_TARGET_COUNT; ++i) {
    const double dx = rawX[i] - meanX;
    const double dy = rawY[i] - meanY;
    const double targetX = TOUCH_CALIBRATION_X[i];
    const double targetY = TOUCH_CALIBRATION_Y[i];
    xx += dx * dx;
    xy += dx * dy;
    yy += dy * dy;
    xTargetX += dx * targetX;
    yTargetX += dy * targetX;
    xTargetY += dx * targetY;
    yTargetY += dy * targetY;
  }
  const double determinant = xx * yy - xy * xy;
  if (xx <= 0 || yy <= 0 ||
      determinant <= 1e-6 * xx * yy) {
    return false;
  }

  fit.xRawX = (yy * xTargetX - xy * yTargetX) / determinant;
  fit.xRawY = (xx * yTargetX - xy * xTargetX) / determinant;
#if defined(BOARD_JC3248W535)
  fit.xOffset = jc3248w535::DISPLAY_WIDTH / 2.0 -
                fit.xRawX * meanX - fit.xRawY * meanY;
#else
  fit.xOffset = DISPLAY_WIDTH / 2.0 -
                fit.xRawX * meanX - fit.xRawY * meanY;
#endif
  fit.yRawX = (yy * xTargetY - xy * yTargetY) / determinant;
  fit.yRawY = (xx * yTargetY - xy * xTargetY) / determinant;
#if defined(BOARD_JC3248W535)
  fit.yOffset = jc3248w535::DISPLAY_HEIGHT / 2.0 -
                fit.yRawX * meanX - fit.yRawY * meanY;
#else
  fit.yOffset = DISPLAY_HEIGHT / 2.0 -
                fit.yRawX * meanX - fit.yRawY * meanY;
#endif
  fit.valid = isfinite(fit.xRawX) && isfinite(fit.xRawY) &&
              isfinite(fit.xOffset) && isfinite(fit.yRawX) &&
              isfinite(fit.yRawY) && isfinite(fit.yOffset);
  return fit.valid;
}

bool isTouchCalibrationValid(const TouchCalibration &calibration) {
  const float determinant =
      calibration.xRawX * calibration.yRawY -
      calibration.xRawY * calibration.yRawX;
  return isfinite(calibration.xRawX) && isfinite(calibration.xRawY) &&
         isfinite(calibration.xOffset) && isfinite(calibration.yRawX) &&
         isfinite(calibration.yRawY) && isfinite(calibration.yOffset) &&
         fabsf(determinant) > 1e-6f;
}

// Draw one calibration target and wait for the user to touch its center.
void drawCalibrationTarget(uint8_t index) {
#if defined(BOARD_JC3248W535)
  display.setNativeCoordinates(true);
#endif
  display.fillScreen(backgroundColor);
  display.setTextDatum(TL_DATUM);
  display.setTextColor(primaryTextColor(), backgroundColor);
  display.drawString("Touch calibration", 24, 4, 2);
  display.drawString(
      "Touch target " + String(index + 1) + "/" +
          String(TOUCH_CALIBRATION_TARGET_COUNT),
      24, 22,
                     2);
  const int16_t targetX = TOUCH_CALIBRATION_X[index];
  const int16_t targetY = TOUCH_CALIBRATION_Y[index];
  display.drawCircle(targetX, targetY, 13, TFT_CYAN);
  display.drawFastHLine(targetX - 18, targetY, 37, TFT_CYAN);
  display.drawFastVLine(targetX, targetY - 18, 37, TFT_CYAN);
#if defined(BOARD_JC3248W535)
  display.flushFrame();
#endif
}

bool touchscreenTouched() {
#if defined(BOARD_JC3248W535)
  uint8_t data[8]{};
  const uint8_t readCommand[11] = {
      0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, sizeof(data), 0x00, 0x00,
      0x00};
  Wire.beginTransmission(TOUCH_ADDRESS);
  Wire.write(readCommand, sizeof(readCommand));
  if (Wire.endTransmission() != 0 ||
      Wire.requestFrom(TOUCH_ADDRESS, sizeof(data)) != sizeof(data)) {
    return false;
  }
  for (uint8_t &byte : data) {
    byte = Wire.read();
  }
  if (data[1] == 0 || data[1] > 1) {
    return false;
  }
  const uint16_t rawX =
      (static_cast<uint16_t>(data[2] & 0x0F) << 8) | data[3];
  const uint16_t rawY =
      (static_cast<uint16_t>(data[4] & 0x0F) << 8) | data[5];
  if (rawX > 500 || rawY > 500) {
    return false;
  }
  lastTouchRawX = rawY;
  lastTouchRawY = 320 - rawX;
  return true;
#elif defined(BOARD_FREENOVE_FNK0114L)
  return display.getTouchRawZ() > 350;
#else
  return touch.touched();
#endif
}

void readRawTouch(uint16_t &x, uint16_t &y) {
#if defined(BOARD_JC3248W535)
  x = lastTouchRawX;
  y = lastTouchRawY;
#elif defined(BOARD_FREENOVE_FNK0114L)
  display.getTouchRaw(&x, &y);
#else
  const TS_Point point = touch.getPoint();
  x = point.x;
  y = point.y;
#endif
}

void waitForTouchRelease() {
  while (touchscreenTouched()) {
    delay(10);
  }
  delay(120);
}

// Collect calibration points and persist the mapping for the active board.
void calibrateTouchscreen() {
  for (;;) {
    uint16_t measuredX[TOUCH_CALIBRATION_TARGET_COUNT]{};
    uint16_t measuredY[TOUCH_CALIBRATION_TARGET_COUNT]{};
    for (uint8_t target = 0; target < TOUCH_CALIBRATION_TARGET_COUNT;
         ++target) {
      drawCalibrationTarget(target);
      waitForTouchRelease();
      while (!touchscreenTouched()) {
        delay(10);
      }
      uint32_t sumX = 0;
      uint32_t sumY = 0;
      uint8_t samples = 0;
      for (uint8_t sample = 0; sample < 8; ++sample) {
        if (touchscreenTouched()) {
          uint16_t rawX = 0;
          uint16_t rawY = 0;
          readRawTouch(rawX, rawY);
          sumX += rawX;
          sumY += rawY;
          ++samples;
        }
        delay(8);
      }
      while (samples == 0) {
        while (!touchscreenTouched()) {
          delay(10);
        }
        for (uint8_t sample = 0; sample < 8; ++sample) {
          if (touchscreenTouched()) {
            uint16_t rawX = 0;
            uint16_t rawY = 0;
            readRawTouch(rawX, rawY);
            sumX += rawX;
            sumY += rawY;
            ++samples;
          }
          delay(8);
        }
      }
      measuredX[target] = sumX / samples;
      measuredY[target] = sumY / samples;
      waitForTouchRelease();
    }

#if defined(BOARD_FREENOVE_FNK0114L)
    const bool calibrationFit =
        fitTouchCalibration(measuredX, measuredY, freenoveTouchCalibration);
#else
    const bool calibrationFit =
        fitTouchCalibration(measuredX, measuredY, touchCalibration);
#endif
    if (calibrationFit) {
      break;
    }

    display.fillScreen(TFT_RED);
    display.setTextColor(TFT_WHITE, TFT_RED);
#if defined(BOARD_JC3248W535)
    display.drawCentreString("Calibration failed",
                             jc3248w535::DISPLAY_WIDTH / 2,
                             jc3248w535::DISPLAY_HEIGHT / 2, 2);
#else
    display.drawCentreString("Calibration failed", DISPLAY_WIDTH / 2,
                             DISPLAY_HEIGHT / 2, 2);
#endif
#if defined(BOARD_JC3248W535)
    display.flushFrame();
#endif
    Serial.println("ERROR: touchscreen calibration range is invalid.");
    delay(1500);
  }

#if defined(BOARD_FREENOVE_FNK0114L)
  const bool saved =
      preferences.putFloat("ftXx", freenoveTouchCalibration.xRawX) > 0 &&
      preferences.putFloat("ftXy", freenoveTouchCalibration.xRawY) > 0 &&
      preferences.putFloat("ftXo", freenoveTouchCalibration.xOffset) > 0 &&
      preferences.putFloat("ftYx", freenoveTouchCalibration.yRawX) > 0 &&
      preferences.putFloat("ftYy", freenoveTouchCalibration.yRawY) > 0 &&
      preferences.putFloat("ftYo", freenoveTouchCalibration.yOffset) > 0 &&
      preferences.putUChar("ftVer", TOUCH_CALIBRATION_VERSION) > 0;
#else
  const bool saved =
      preferences.putFloat("touchXx", touchCalibration.xRawX) > 0 &&
      preferences.putFloat("touchXy", touchCalibration.xRawY) > 0 &&
      preferences.putFloat("touchXo", touchCalibration.xOffset) > 0 &&
      preferences.putFloat("touchYx", touchCalibration.yRawX) > 0 &&
      preferences.putFloat("touchYy", touchCalibration.yRawY) > 0 &&
      preferences.putFloat("touchYo", touchCalibration.yOffset) > 0 &&
      preferences.putUChar("touchVer", TOUCH_CALIBRATION_VERSION) > 0;
#endif
  if (!saved) {
    Serial.println("ERROR: could not save touchscreen calibration.");
  } else {
#if defined(BOARD_FREENOVE_FNK0114L)
    freenoveTouchCalibration.valid = true;
#else
    touchCalibration.valid = true;
#endif
  }
#if defined(BOARD_JC3248W535)
  display.setNativeCoordinates(true);
#endif
}

bool beginTouchPress(bool touched) {
  if (!touched) {
    if (touchHeld) {
      const uint32_t now = millis();
      if (!touchReleasePending) {
        touchReleaseStartedAt = now;
        touchReleasePending = true;
      } else if (now - touchReleaseStartedAt >=
                 TOUCH_RELEASE_DEBOUNCE_MS) {
        touchHeld = false;
        touchReleasePending = false;
      }
    }
    return false;
  }

  touchReleasePending = false;
  if (touchHeld) {
    return false;
  }
  touchHeld = true;
  return true;
}

bool readTouchPoint(int16_t &x, int16_t &y) {
#if defined(BOARD_FREENOVE_FNK0114L)
  if (!beginTouchPress(display.getTouchRawZ() >= 350)) {
    return false;
  }
  const uint32_t now = millis();
  if (now - lastTouchAt < TOUCH_DEBOUNCE_MS) {
    return false;
  }
  lastTouchAt = now;
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  display.getTouchRaw(&rawX, &rawY);
  x = constrain(lroundf(freenoveTouchCalibration.xRawX * rawX +
                        freenoveTouchCalibration.xRawY * rawY +
                        freenoveTouchCalibration.xOffset),
                0, display.width() - 1);
  y = constrain(lroundf(freenoveTouchCalibration.yRawX * rawX +
                        freenoveTouchCalibration.yRawY * rawY +
                        freenoveTouchCalibration.yOffset),
                0, display.height() - 1);
  return true;
#elif defined(BOARD_JC3248W535)
  if (!beginTouchPress(touchscreenTouched())) {
    return false;
  }

  const uint32_t now = millis();
  if (now - lastTouchAt < TOUCH_DEBOUNCE_MS) {
    return false;
  }
  lastTouchAt = now;
  uint32_t sumX = 0;
  uint32_t sumY = 0;
  uint8_t samples = 0;
  for (uint8_t i = 0; i < 3; ++i) {
    uint16_t rawX = 0;
    uint16_t rawY = 0;
    readRawTouch(rawX, rawY);
    sumX += rawX;
    sumY += rawY;
    ++samples;
    delayMicroseconds(1200);
    if (!touchscreenTouched()) {
      break;
    }
  }
  if (samples == 0) {
    return false;
  }
  const float rawX = sumX / static_cast<float>(samples);
  const float rawY = sumY / static_cast<float>(samples);
  const int32_t mappedX = lroundf(touchCalibration.xRawX * rawX +
                                  touchCalibration.xRawY * rawY +
                                  touchCalibration.xOffset);
  const int32_t mappedY = lroundf(touchCalibration.yRawX * rawX +
                                  touchCalibration.yRawY * rawY +
                                  touchCalibration.yOffset);
  x = constrain(mappedX, 0, jc3248w535::DISPLAY_WIDTH - 1);
  y = constrain(mappedY, 0, jc3248w535::DISPLAY_HEIGHT - 1);
  return true;
#else
  if (!beginTouchPress(touchscreenTouched())) {
    return false;
  }

  const uint32_t now = millis();
  if (now - lastTouchAt < TOUCH_DEBOUNCE_MS) {
    return false;
  }
  lastTouchAt = now;

  uint32_t sumX = 0;
  uint32_t sumY = 0;
  uint8_t samples = 0;
  for (uint8_t i = 0; i < 3 && touchscreenTouched(); ++i) {
    uint16_t rawX = 0;
    uint16_t rawY = 0;
    readRawTouch(rawX, rawY);
    sumX += rawX;
    sumY += rawY;
    ++samples;
    delayMicroseconds(1200);
  }
  if (samples == 0) {
    return false;
  }
  const float rawX = sumX / static_cast<float>(samples);
  const float rawY = sumY / static_cast<float>(samples);
  const int32_t mappedX = lroundf(touchCalibration.xRawX * rawX +
                                  touchCalibration.xRawY * rawY +
                                  touchCalibration.xOffset);
  const int32_t mappedY = lroundf(touchCalibration.yRawX * rawX +
                                  touchCalibration.yRawY * rawY +
                                  touchCalibration.yOffset);
  x = constrain(mappedX, 0, display.width() - 1);
  y = constrain(mappedY, 0, display.height() - 1);
  return true;
#endif
}

// Update the home-screen status and schedule a redraw.
void setStatus(const String &message) {
  statusMessage = message;
  markUiDirty();
}

uint8_t stationPageCount() {
  return max<uint8_t>(
      1, (stationCount + STATIONS_PER_PAGE - 1) / STATIONS_PER_PAGE);
}

void showStationPageFor(uint8_t index) {
  stationListPage = index / STATIONS_PER_PAGE;
}

bool saveVolume() {
  return preferences.putUChar("volume", volume) > 0;
}

bool loadStationUrl(uint8_t index, char *url, size_t urlSize) {
  char urlKey[5];
  snprintf(urlKey, sizeof(urlKey), "u%u", index);
  return preferences.getString(urlKey, url, urlSize) > 0;
}

bool saveStationUrl(uint8_t index, const char *url) {
  char urlKey[5];
  snprintf(urlKey, sizeof(urlKey), "u%u", index);
  return preferences.putString(urlKey, url) > 0;
}

bool shiftStationUrls(uint8_t index, uint8_t count) {
  char url[STATION_URL_SIZE];
  for (uint8_t current = index; current + 1 < count; ++current) {
    if (!loadStationUrl(current + 1, url, sizeof(url)) ||
        !saveStationUrl(current, url)) {
      Serial.printf("ERROR: could not shift saved URL for station %u.\n",
                    current + 1);
      return false;
    }
  }
  char lastUrlKey[5];
  snprintf(lastUrlKey, sizeof(lastUrlKey), "u%u", count - 1);
  if (!preferences.remove(lastUrlKey)) {
    Serial.println("ERROR: could not remove final saved station URL.");
    return false;
  }
  return true;
}

bool saveStationList() {
  bool saved = true;
  for (uint8_t i = 0; i < stationCount; ++i) {
    char nameKey[5];
    snprintf(nameKey, sizeof(nameKey), "n%u", i);
    saved &= preferences.putString(nameKey, stations[i].name) > 0;
    char url[STATION_URL_SIZE];
    saved &= loadStationUrl(i, url, sizeof(url));
  }
  saved &= preferences.putUChar("count", stationCount) > 0;
  if (!saved) {
    setStatus("Could not save station list");
    Serial.println("ERROR: could not persist the station list.");
  }
  return saved;
}

// Populate the station array from the compiled-in defaults.
void loadDefaultStations() {
  stationCount = DEFAULT_STATION_COUNT;
  for (uint8_t i = 0; i < stationCount; ++i) {
    strlcpy(stations[i].name, DEFAULT_STATION_NAMES[i],
            sizeof(stations[i].name));
    if (!saveStationUrl(i, DEFAULT_STATION_URLS[i])) {
      Serial.printf("ERROR: could not save default station URL %u.\n", i + 1);
    }
  }
}

// Detect the old factory list so it can be migrated to the current defaults.
bool isLegacyDefaultPlaylist() {
  if (stationCount != 3) {
    return false;
  }
  char url[STATION_URL_SIZE];
  return strcmp(stations[0].name, "Radio Paradise") == 0 &&
         loadStationUrl(0, url, sizeof(url)) &&
         strcmp(url, "http://stream.radioparadise.com/mp3-128") == 0 &&
         strcmp(stations[1].name, "SomaFM Groove Salad") == 0 &&
         loadStationUrl(1, url, sizeof(url)) &&
         strcmp(url, "https://ice1.somafm.com/groovesalad-128-mp3") == 0 &&
         strcmp(stations[2].name, "Deutschlandfunk") == 0 &&
         loadStationUrl(2, url, sizeof(url)) &&
         strcmp(url,
                "https://st01.sslstream.dlf.de/dlf/01/128/mp3/stream.mp3") ==
             0;
}

// Update unchanged factory URLs in saved lists without replacing user edits.
bool migrateDefaultStationUrls() {
  struct UrlMigration {
    const char *name;
    const char *oldUrl;
  };
  constexpr UrlMigration migrations[] = {
      {"WDR 2 (DE)",
       "http://wdr-wdr2-rkn.icecast.wdr.de/wdr/wdr2/rkn/mp3/128/stream.mp3"},
      {"SWR3 (DE)",
       "http://swr-swr3-live.cast.addradio.de/swr/swr3/live/mp3/128/stream.mp3"},
      {"KEXP Seattle (USA)",
       "https://kexp-mp3-128.streamguys1.com/kexp128.mp3"},
      {"BBC Radio 1 (UK)", "http://stream.live.vc.bbc.co.uk/bbc_radio_one"},
      {"BBC Radio 2 (UK)", "http://stream.live.vc.bbc.co.uk/bbc_radio_two"},
      {"Capital FM London (UK)",
       "https://stream-capital.musicradio.com/CapitalLondonMP3"},
      {"Heart London (UK)",
       "https://stream-heart.musicradio.com/HeartLondonMP3"},
  };
  constexpr const char *factoryStationNames[] = {
      "1LIVE (DE)", "Antenne Bayern (DE)", "WDR 2 (DE)", "SWR3 (DE)",
      "Hit Radio FFH (DE)", "Bayern 3 (DE)", "Sunshine Live (DE)",
      "Absolut Radio (DE)", "N-JOY (DE)", "bigFM (DE)",
      "Radio BOB! (DE)", "Klassik Radio (DE)", "RPR1 (DE)",
      "STAR FM Berlin (DE)", "MDR JUMP (DE)",
      "WHTZ Z100 New York (USA)", "KIIS-FM Los Angeles (USA)",
      "KEXP Seattle (USA)", "WFAN New York (USA)", "WLTW Lite FM NY (USA)",
      "BBC Radio 1 (UK)", "BBC Radio 2 (UK)", "Capital FM London (UK)",
      "Heart London (UK)", "Smooth Radio (UK)", "Absolute Radio (UK)",
  };
  bool mergeFactoryStations =
      stationCount == 12 || stationCount == 14 || stationCount == 20;
  for (uint8_t stationIndex = 0;
       mergeFactoryStations && stationIndex < stationCount; ++stationIndex) {
    bool knownStation = false;
    for (const char *name : factoryStationNames) {
      knownStation |= strcmp(stations[stationIndex].name, name) == 0;
    }
    for (uint8_t previous = 0; previous < stationIndex; ++previous) {
      if (strcmp(stations[stationIndex].name, stations[previous].name) == 0) {
        knownStation = false;
        break;
      }
    }
    mergeFactoryStations &= knownStation;
  }
  bool changed = false;
  for (uint8_t stationIndex = 0; stationIndex < stationCount;) {
    char url[STATION_URL_SIZE];
    if (!loadStationUrl(stationIndex, url, sizeof(url))) {
      Serial.printf("ERROR: saved URL for station %u is unavailable.\n",
                    stationIndex + 1);
      return changed;
    }
    constexpr struct {
      const char *name;
      const char *url;
    } englishFactoryStations[] = {
        {"WHTZ Z100 New York (USA)", "https://stream.revma.ihrhls.com/zc1469"},
        {"KIIS-FM Los Angeles (USA)", "https://stream.revma.ihrhls.com/zc185"},
        {"KEXP Seattle (USA)", "https://kexp.streamguys1.com/kexp64.aac"},
        {"KEXP Seattle (USA)",
         "https://kexp-mp3-128.streamguys1.com/kexp128.mp3"},
        {"WFAN New York (USA)", "https://stream.revma.ihrhls.com/zc5231"},
        {"WLTW Lite FM NY (USA)", "https://stream.revma.ihrhls.com/zc1477"},
        {"Capital FM London (UK)", "https://media-ice.musicradio.com/CapitalMP3"},
        {"Capital FM London (UK)",
         "https://stream-capital.musicradio.com/CapitalLondonMP3"},
        {"Heart London (UK)",
         "https://media-ice.musicradio.com/HeartLondonMP3"},
        {"Heart London (UK)",
         "https://stream-heart.musicradio.com/HeartLondonMP3"},
        {"Smooth Radio (UK)", "https://media-ice.musicradio.com/SmoothUKMP3"},
        {"Absolute Radio (UK)",
         "https://icecast.bauerstreaming.com/absolute-radio-mp3"},
    };
    bool isEnglishFactoryStation = false;
    for (const auto &entry : englishFactoryStations) {
      isEnglishFactoryStation |=
          strcmp(stations[stationIndex].name, entry.name) == 0 &&
          strcmp(url, entry.url) == 0;
    }
    const bool isDefaultBbcOne =
        strcmp(stations[stationIndex].name, "BBC Radio 1 (UK)") == 0 &&
        (strcmp(url,
                "http://stream.live.vc.bbc.co.uk/bbc_radio_one") == 0 ||
         strcmp(url,
                "https://lsn.lv/bbcradio.m3u8?station=bbc_radio_one&bitrate=96000") ==
             0);
    const bool isDefaultBbcTwo =
        strcmp(stations[stationIndex].name, "BBC Radio 2 (UK)") == 0 &&
        (strcmp(url,
                "http://stream.live.vc.bbc.co.uk/bbc_radio_two") == 0 ||
         strcmp(url,
                "https://lsn.lv/bbcradio.m3u8?station=bbc_radio_two&bitrate=96000") ==
             0);
    if (isDefaultBbcOne || isDefaultBbcTwo || isEnglishFactoryStation) {
      if (!shiftStationUrls(stationIndex, stationCount)) {
        return changed;
      }
      for (uint8_t next = stationIndex; next + 1 < stationCount; ++next) {
        stations[next] = stations[next + 1];
      }
      --stationCount;
      if (selectedStation > stationIndex) {
        --selectedStation;
      }
      changed = true;
      continue;
    }
    for (const UrlMigration &migration : migrations) {
      if (strcmp(stations[stationIndex].name, migration.name) != 0 ||
          strcmp(url, migration.oldUrl) != 0) {
        continue;
      }
      for (uint8_t defaultIndex = 0; defaultIndex < DEFAULT_STATION_COUNT;
           ++defaultIndex) {
        if (strcmp(DEFAULT_STATION_NAMES[defaultIndex], migration.name) == 0) {
          if (!saveStationUrl(stationIndex,
                              DEFAULT_STATION_URLS[defaultIndex])) {
            Serial.printf("ERROR: could not update saved URL for %s.\n",
                          migration.name);
            return changed;
          }
          changed = true;
          break;
        }
      }
    }
    ++stationIndex;
  }
  if (mergeFactoryStations) {
    for (uint8_t defaultIndex = 0; defaultIndex < DEFAULT_STATION_COUNT;
         ++defaultIndex) {
      bool alreadyPresent = false;
      char url[STATION_URL_SIZE];
      for (uint8_t stationIndex = 0; stationIndex < stationCount;
           ++stationIndex) {
        alreadyPresent |=
            strcmp(stations[stationIndex].name,
                   DEFAULT_STATION_NAMES[defaultIndex]) == 0;
        if (loadStationUrl(stationIndex, url, sizeof(url))) {
          alreadyPresent |=
              strcmp(url, DEFAULT_STATION_URLS[defaultIndex]) == 0;
        }
      }
      if (alreadyPresent) {
        continue;
      }
      if (stationCount >= MAX_STATIONS) {
        Serial.println("ERROR: station list is full; not all defaults fit.");
        break;
      }
      strlcpy(stations[stationCount].name, DEFAULT_STATION_NAMES[defaultIndex],
              sizeof(stations[stationCount].name));
      if (!saveStationUrl(stationCount, DEFAULT_STATION_URLS[defaultIndex])) {
        Serial.printf("ERROR: could not save default station URL %u.\n",
                      defaultIndex + 1);
        break;
      }
      ++stationCount;
      changed = true;
    }
  }
  if (changed && selectedStation >= stationCount) {
    selectedStation = stationCount == 0 ? 0 : stationCount - 1;
  }
  return changed;
}

// Load persisted stations, repair incomplete data, and migrate legacy entries.
void loadStations() {
  if (!preferences.isKey("count")) {
    loadDefaultStations();
    saveStationList();
  } else {
    stationCount =
        min<uint8_t>(preferences.getUChar("count", 0), MAX_STATIONS);
    for (uint8_t i = 0; i < stationCount; ++i) {
      char nameKey[5];
      char urlKey[5];
      snprintf(nameKey, sizeof(nameKey), "n%u", i);
      snprintf(urlKey, sizeof(urlKey), "u%u", i);
      preferences.getString(nameKey, stations[i].name,
                            sizeof(stations[i].name));
      char url[STATION_URL_SIZE];
      preferences.getString(urlKey, url, sizeof(url));
      if (stations[i].name[0] == '\0' || url[0] == '\0') {
        Serial.printf("ERROR: saved station %u is incomplete.\n", i + 1);
        stationCount = i;
        saveStationList();
        break;
      }
    }
    if (isLegacyDefaultPlaylist()) {
      loadDefaultStations();
      saveStationList();
    } else if (migrateDefaultStationUrls()) {
      saveStationList();
      if (preferences.putUChar("selected", selectedStation) == 0) {
        Serial.println("ERROR: could not save selected station after migration.");
      }
    }
  }
  if (stationCount == 0) {
    setStatus("No stations available");
  }
}

// Apply a bounded volume setting to the decoder and save it to preferences.
void setVolume(uint8_t newVolume) {
  volume = min<uint8_t>(newVolume, 21);
  setRadioVolume(volume);
  if (!saveVolume()) {
    setStatus("Could not save volume");
  } else {
    markUiDirty();
  }
}

// Apply the optional equalizer boost and persist the user's choice.
void setAudioBoost(bool enabled) {
  audioBoost = enabled;
  const int8_t gain = audioBoost ? AUDIO_BOOST_DB : 0;
  setRadioTone(gain, gain, gain);
  if (preferences.putBool("boost", audioBoost) == 0) {
    Serial.println("ERROR: could not save audio boost setting.");
    setStatus("Could not save audio boost");
    return;
  }
  setStatus(audioBoost ? "Audio boost enabled" : "Audio boost disabled");
}

// Toggle spectrum capture and persist the display preference.
void setAudioSpectrum(bool enabled) {
  audioSpectrumEnabled = enabled;
  setAudioSpectrumEnabled(enabled);
  if (preferences.putBool("spectrum", audioSpectrumEnabled) == 0) {
    Serial.println("ERROR: could not save spectrum setting.");
    setStatus("Could not save spectrum setting");
    return;
  }
  markUiDirty();
}

// Toggle the audio compressor and persist the processing preference.
void setAudioCompressor(bool enabled) {
  audioCompressorEnabled = enabled;
  setAudioCompressorEnabled(enabled);
  if (preferences.putBool("compressor", audioCompressorEnabled) == 0) {
    Serial.println("ERROR: could not save audio compression setting.");
    setStatus("Could not save compression setting");
    return;
  }
  setStatus(audioCompressorEnabled ? "Audio compression enabled"
                                   : "Audio compression disabled");
}

// Toggle DAC noise shaping and persist the processing preference.
void setAudioNoiseShaping(bool enabled) {
  audioNoiseShaping = enabled;
  setAudioNoiseShapingEnabled(enabled);
  if (preferences.putBool("noiseshape", audioNoiseShaping) == 0) {
    Serial.println("ERROR: could not save noise shaping setting.");
    setStatus("Could not save noise shaping setting");
    return;
  }
  setStatus(audioNoiseShaping ? "Noise shaping enabled"
                              : "Noise shaping disabled");
}

// Control the optional active-low amplifier enable on compatible CYD boards.
void setAudioAmpEnabled(bool enabled) {
  audioAmpEnabled = enabled;
#if !defined(BOARD_JC3248W535)
  digitalWrite(AUDIO_AMP_ENABLE_PIN, enabled ? LOW : HIGH);
  pinMode(AUDIO_AMP_ENABLE_PIN, OUTPUT);
#endif
  if (preferences.putBool("amp4", audioAmpEnabled) == 0) {
    Serial.println("ERROR: could not save GPIO 4 amplifier setting.");
    setStatus("Could not save GPIO 4 setting");
    return;
  }
  setStatus(enabled ? "GPIO 4 amplifier enabled"
                    : "GPIO 4 amplifier disabled");
}

// Restart the selected stream after checking timer and Wi-Fi constraints.
void startStream() {
  if (stationCount == 0) {
    setStatus("No stations available");
    return;
  }
  if (timerState == TimerState::Expired ||
      (timerState == TimerState::Running &&
       timerRemainingMs(millis()) <= TIMER_MUSIC_CUTOFF_MS)) {
    setStatus("Timer: music stopped");
    return;
  }
  if (WiFi.status() != WL_CONNECTED) {
    setStatus(wifiSsid.isEmpty() ? "Set up Wi-Fi" : "Wi-Fi not connected");
    return;
  }

  streamTitle = "";
  streamStation = "";
  streamRestartPending = false;
  streamActive = false;
  stopRadioSong();
  setRadioVolume(volume);
  startAudioFadeIn();
  char stationUrl[STATION_URL_SIZE];
  if (!loadStationUrl(selectedStation, stationUrl, sizeof(stationUrl))) {
    streamActive = false;
    Serial.printf("ERROR: could not load URL for station %u.\n",
                  selectedStation + 1);
    setStatus("Could not load station URL");
    return;
  }
  if (!connectRadioToHost(stationUrl)) {
    streamActive = false;
    setStatus("Could not start stream");
    Serial.printf("ERROR: could not connect to station %u.\n",
                  selectedStation + 1);
    return;
  }
  streamActive = true;
  setStatus("Connecting to stream...");
}

// Save a station selection and either start its stream or connect to Wi-Fi.
void selectStation(uint8_t index) {
  if (index >= stationCount) {
    return;
  }
  selectedStation = index;
  streamTitle = "";
  streamStation = "";
  if (preferences.putUChar("selected", selectedStation) == 0) {
    Serial.println("ERROR: could not save selected station.");
  }
  currentScreen = Screen::Home;
  if (WiFi.status() == WL_CONNECTED) {
    startStream();
  } else if (!wifiSsid.isEmpty()) {
    startWifiConnection();
  } else {
    setStatus("Set up Wi-Fi");
  }
  markUiDirty();
}

void beginWifiConnection() {
  if (wifiSsid.isEmpty()) {
    setStatus("Enter an SSID first");
    return;
  }
  stopRadioSong();
  streamActive = false;
  startWifiConnection();
}

// Save Wi-Fi credentials and report storage failures to the user and log.
bool saveWifiCredentials() {
  const bool savedSsid = preferences.putString("ssid", wifiSsid) > 0;
  const bool savedPassword =
      preferences.putString("password", wifiPassword) > 0;
  if (!savedSsid || !savedPassword) {
    Serial.println("ERROR: could not save Wi-Fi credentials.");
    setStatus("Could not save Wi-Fi credentials");
    return false;
  }
  return true;
}

// Begin Wi-Fi association and arrange to start playback when connected.
void startWifiConnection() {
  if (wifiPassword.length() < 8 || wifiPassword.length() > 63) {
    wifiConnecting = false;
    playWhenConnected = false;
    setStatus("WPA2 password must be 8-63 characters");
    return;
  }

  wifiDisconnectReason = 0;
  wifiConnecting = true;
  playWhenConnected = true;
  wifiConnectStartedAt = millis();
  setStatus("Connecting to Wi-Fi...");

  const wl_status_t result =
      WiFi.begin(wifiSsid.c_str(), wifiPassword.c_str());
  if (result == WL_CONNECT_FAILED) {
    wifiConnecting = false;
    playWhenConnected = false;
    setStatus("Could not start Wi-Fi");
    Serial.println("ERROR: Wi-Fi rejected the connection request.");
  }
}

// Translate common Wi-Fi disconnect causes into actionable UI messages.
String wifiFailureMessage(wl_status_t status, uint8_t reason) {
  if (reason == WIFI_REASON_NO_AP_FOUND || status == WL_NO_SSID_AVAIL) {
    return "SSID not found (check 2.4 GHz network)";
  }
  if (reason == WIFI_REASON_AUTH_FAIL ||
      reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
      reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
      status == WL_CONNECT_FAILED) {
    return "Authentication failed: check WPA2 password";
  }
  if (reason == WIFI_REASON_ASSOC_FAIL) {
    return "Router rejected the connection";
  }
  if (status == WL_CONNECTION_LOST) {
    return "Wi-Fi connection lost";
  }
  return "Wi-Fi error " + String(reason) + " (status " +
         String(static_cast<int>(status)) + ")";
}

// Process asynchronous Wi-Fi events, timeouts, and automatic playback startup.
void updateNetwork() {
  static uint32_t wifiLostAt = 0;
  if (WiFi.status() == WL_CONNECTED) {
    wifiLostAt = 0;
    if (wifiConnecting) {
      wifiConnecting = false;
      wifiDisconnectReason = 0;
      configTzTime(BERLIN_TIMEZONE, "pool.ntp.org", "time.google.com");
      setStatus("Wi-Fi connected");
      if (playWhenConnected) {
        playWhenConnected = false;
        startStream();
      }
    }
    return;
  }

  // Restart the connection (and stream) when Wi-Fi stays down for 30 s.
  if (wifiConnecting || wifiSsid.isEmpty()) {
    wifiLostAt = 0;
  } else if (wifiLostAt == 0) {
    wifiLostAt = millis();
  } else if (millis() - wifiLostAt >= WIFI_RECONNECT_DELAY_MS) {
    wifiLostAt = 0;
    WiFi.disconnect();
    startWifiConnection();
    return;
  }

  if (wifiConnecting && millis() - wifiConnectStartedAt > WIFI_TIMEOUT_MS) {
    wifiConnecting = false;
    playWhenConnected = false;
    const wl_status_t status = WiFi.status();
    const uint8_t reason = wifiDisconnectReason;
    setStatus(wifiFailureMessage(status, reason));
    Serial.printf("ERROR: Wi-Fi connection timed out (status %d, reason %u).\n",
                  static_cast<int>(status), reason);
  }
}

// Load an existing station or prepare a blank entry for editing.
void startStationEditor(bool isNew, uint8_t index) {
  stationEditorIsNew = isNew;
  editorIndex = index;
  editName = isNew ? "" : stations[index].name;
  char url[STATION_URL_SIZE] = "";
  if (!isNew && !loadStationUrl(index, url, sizeof(url))) {
    Serial.printf("ERROR: could not load URL for station %u.\n", index + 1);
    setStatus("Could not load station URL");
    return;
  }
  editUrl = isNew ? "" : url;
  if (isNew) {
    showStationPageFor(index);
  }
  currentScreen = Screen::StationEditor;
  markUiDirty();
}

// Open the keyboard with the selected field's current value and return screen.
void beginKeyboard(KeyboardTarget target) {
  keyboardTarget = target;
  keyboardReturnScreen = currentScreen;
  switch (target) {
    case KeyboardTarget::Ssid:
      keyboardValue = wifiSsid;
      break;
    case KeyboardTarget::Password:
      keyboardValue = wifiPassword;
      break;
    case KeyboardTarget::StationName:
      keyboardValue = editName;
      break;
    case KeyboardTarget::StationUrl:
      keyboardValue = editUrl;
      break;
  }
  keyboardPage = 0;
  keyboardUppercase = false;
  currentScreen = Screen::Keyboard;
  markUiDirty();
}

// Commit or discard keyboard input, then return to the originating screen.
void finishKeyboard(bool accept) {
  if (accept) {
    switch (keyboardTarget) {
      case KeyboardTarget::Ssid:
        wifiSsid = keyboardValue;
        break;
      case KeyboardTarget::Password:
        wifiPassword = keyboardValue;
        break;
      case KeyboardTarget::StationName:
        editName = keyboardValue;
        break;
      case KeyboardTarget::StationUrl:
        editUrl = keyboardValue;
        break;
    }
  }
  currentScreen = keyboardReturnScreen;
  markUiDirty();
}

uint16_t maximumKeyboardLength() {
  switch (keyboardTarget) {
    case KeyboardTarget::Ssid:
      return 32;
    case KeyboardTarget::Password:
      return 63;
    case KeyboardTarget::StationName:
      return STATION_NAME_SIZE - 1;
    case KeyboardTarget::StationUrl:
      return STATION_URL_SIZE - 1;
  }
  return 0;
}

void appendKeyboardCharacter(char value) {
  if (keyboardValue.length() < maximumKeyboardLength()) {
    keyboardValue += value;
    markUiDirty();
  }
}

// Map keyboard touches to characters, mode changes, deletion, and actions.
void handleKeyboardTouch(int16_t x, int16_t y) {
  const int16_t showWidth = wideKeyboardLayout() ? 70 : 62;
  if (keyboardTarget == KeyboardTarget::Password &&
      pointInRect(x, y, UI_LEFT + UI_WIDTH - showWidth - 8, 26,
                  showWidth + 16, 44)) {
    showWifiPassword = !showWifiPassword;
    markUiDirty();
    return;
  }

  for (uint8_t row = 0; row < 3; ++row) {
    const String keys = keyboardRow(row);
    if (keys.isEmpty()) {
      continue;
    }
    const int16_t keyGap = keyboardKeyGap();
    const int16_t keyWidth = keyboardKeyWidth(keys);
    const int16_t rowWidth = keys.length() * keyWidth +
                             (keys.length() - 1) * keyGap;
    const int16_t rowLeft = UI_LEFT + (UI_WIDTH - rowWidth) / 2;
    for (uint8_t column = 0; column < keys.length(); ++column) {
      const int16_t keyX = rowLeft + column * (keyWidth + keyGap);
      const int16_t keyY = keyboardRowY(row);
      if (pointInRect(x, y, keyX, keyY, keyWidth, keyboardKeyHeight())) {
        char value = keys[column];
        if (keyboardPage == 0 && keyboardUppercase) {
          value = static_cast<char>(value - 'a' + 'A');
        }
        appendKeyboardCharacter(value);
        return;
      }
    }
  }

  const int16_t controlY = keyboardControlY();
  const int16_t controlHeight = keyboardControlHeight();
  int16_t controlX = 0;
  int16_t controlWidth = 0;
  keyboardControlBounds(0, controlX, controlWidth);
  if (pointInRect(x, y, controlX, controlY, controlWidth, controlHeight)) {
    if (keyboardPage == 0) {
      keyboardUppercase = !keyboardUppercase;
    } else {
      keyboardPage = 0;
    }
  } else {
    keyboardControlBounds(1, controlX, controlWidth);
    if (pointInRect(x, y, controlX, controlY, controlWidth, controlHeight)) {
      keyboardPage = keyboardPage == 0 ? 1 : keyboardPage == 1 ? 2 : 1;
    } else {
      keyboardControlBounds(2, controlX, controlWidth);
      if (pointInRect(x, y, controlX, controlY, controlWidth,
                      controlHeight)) {
        appendKeyboardCharacter(' ');
      } else {
        keyboardControlBounds(3, controlX, controlWidth);
        if (pointInRect(x, y, controlX, controlY, controlWidth,
                        controlHeight) &&
            !keyboardValue.isEmpty()) {
          keyboardValue.remove(keyboardValue.length() - 1);
        }
      }
    }
  }

  const int16_t actionY = keyboardActionY();
  const int16_t actionHeight = keyboardActionHeight();
  const int16_t actionWidth = wideKeyboardLayout() ? (UI_WIDTH - 8) / 2 : 98;
  const int16_t actionGap = 8;
  if (pointInRect(x, y, UI_LEFT, actionY, actionWidth, actionHeight)) {
    finishKeyboard(false);
    return;
  } else if (pointInRect(x, y, UI_LEFT + actionWidth + actionGap, actionY,
                         wideKeyboardLayout() ? actionWidth : 104,
                         actionHeight)) {
    finishKeyboard(true);
    return;
  }
  markUiDirty();
}

// Validate and persist the edited station, restarting it if it was playing.
void saveStationEditor() {
  editName.trim();
  editUrl.trim();
  if (editName.isEmpty() || editUrl.isEmpty() ||
      !(editUrl.startsWith("http://") || editUrl.startsWith("https://"))) {
    setStatus("Enter a name and valid HTTP(S) URL");
    return;
  }

  const uint8_t index = stationEditorIsNew ? stationCount : editorIndex;
  const bool changedPlayingStation =
      !stationEditorIsNew && streamActive && index == selectedStation;
  if (index >= MAX_STATIONS) {
    setStatus("Station list is full");
    return;
  }
  if (!saveStationUrl(index, editUrl.c_str())) {
    Serial.printf("ERROR: could not save URL for station %u.\n", index + 1);
    setStatus("Could not save station URL");
    return;
  }
  strlcpy(stations[index].name, editName.c_str(), sizeof(stations[index].name));
  if (stationEditorIsNew) {
    ++stationCount;
    selectedStation = index;
  }
  const bool saved = saveStationList();
  currentScreen = Screen::Stations;
  if (saved) {
    setStatus("Station saved");
    if (changedPlayingStation) {
      startStream();
    }
  }
}

// Remove a station, repair the selected index, and restart playback if needed.
void deleteStation(uint8_t index) {
  if (index >= stationCount) {
    deleteConfirmation = false;
    return;
  }
  const bool deletedCurrentStation = streamActive && index == selectedStation;
  if (!shiftStationUrls(index, stationCount)) {
    setStatus("Could not update station storage");
    return;
  }
  for (uint8_t i = index; i + 1 < stationCount; ++i) {
    stations[i] = stations[i + 1];
  }
  --stationCount;
  if (stationListPage >= stationPageCount()) {
    stationListPage = stationPageCount() - 1;
  }
  if (stationCount == 0) {
    selectedStation = 0;
    streamActive = false;
    streamTitle = "";
    streamStation = "";
    stopRadioSong();
  } else if (index < selectedStation) {
    --selectedStation;
  } else if (index == selectedStation && selectedStation >= stationCount) {
    selectedStation = stationCount - 1;
  }
  const bool saved = saveStationList();
  if (preferences.putUChar("selected", selectedStation) == 0) {
    Serial.println("ERROR: could not save selected station.");
  }
  deleteConfirmation = false;
  if (saved) {
    setStatus("Station deleted");
  }
  if (deletedCurrentStation && stationCount > 0) {
    startStream();
  }
}

// Convert the slider's horizontal touch coordinate into the 0-21 volume scale.
void adjustVolumeFromTouch(int16_t x) {
#if defined(BOARD_JC3248W535)
  const int16_t relativeX = constrain(x - UI_LEFT - 4, 0, UI_WIDTH - 8);
  const uint8_t newVolume =
      static_cast<uint8_t>((relativeX * 21 + (UI_WIDTH - 8) / 2) /
                           (UI_WIDTH - 8));
#else
  const int16_t relativeX = constrain(x - UI_LEFT - 2, 0, UI_WIDTH - 4);
  const uint8_t newVolume =
      static_cast<uint8_t>((relativeX * 21 + (UI_WIDTH - 4) / 2) /
                           (UI_WIDTH - 4));
#endif
  setVolume(newVolume);
}

#if defined(BOARD_JC3248W535)
void adjustDisplayBrightnessFromTouch(int16_t x) {
  const int16_t relativeX = constrain(x - UI_LEFT, 0, UI_WIDTH - 1);
  const uint16_t brightnessRange =
      DISPLAY_BRIGHTNESS_MAX - DISPLAY_BRIGHTNESS_MIN;
  const uint8_t newBrightness = static_cast<uint8_t>(
      DISPLAY_BRIGHTNESS_MIN +
      (relativeX * brightnessRange + (UI_WIDTH - 1) / 2) / (UI_WIDTH - 1));
  setDisplayBrightness(newBrightness);
}
#endif

// Handle playback navigation and controls on the home screen.
void handleHomeTouch(int16_t x, int16_t y) {
#if defined(BOARD_JC3248W535)
  if (pointInRect(x, y, UI_LEFT + HOME_PREVIOUS_X, HOME_PLAYBACK_Y,
                  HOME_PREVIOUS_WIDTH, HOME_PLAYBACK_HEIGHT)) {
    if (stationCount > 0) {
      selectStation((selectedStation + stationCount - 1) % stationCount);
    }
  } else if (pointInRect(x, y, UI_LEFT + HOME_PLAY_X, HOME_PLAYBACK_Y,
                         HOME_PLAY_WIDTH, HOME_PLAYBACK_HEIGHT)) {
    if (streamActive) {
      stopRadioSong();
      streamActive = false;
      setStatus("Paused");
    } else {
      startStream();
    }
  } else if (pointInRect(x, y, UI_LEFT + HOME_NEXT_X, HOME_PLAYBACK_Y,
                         HOME_NEXT_WIDTH, HOME_PLAYBACK_HEIGHT)) {
    if (stationCount > 0) {
      selectStation((selectedStation + 1) % stationCount);
    }
  } else if (pointInRect(x, y, UI_LEFT - 1, HOME_VOLUME_SLIDER_Y - 8,
                         UI_WIDTH + 2, 28)) {
    adjustVolumeFromTouch(x);
  } else if (pointInRect(x, y, UI_LEFT, HOME_BOTTOM_BUTTON_Y,
                         HOME_BOTTOM_BUTTON_WIDTH,
                         HOME_BOTTOM_BUTTON_HEIGHT)) {
    pressTimerButton(millis());
  } else if (pointInRect(x, y,
                         UI_LEFT + HOME_BOTTOM_BUTTON_WIDTH +
                             HOME_BOTTOM_BUTTON_GAP,
                         HOME_BOTTOM_BUTTON_Y, HOME_BOTTOM_BUTTON_WIDTH,
                         HOME_BOTTOM_BUTTON_HEIGHT)) {
    currentScreen = Screen::Settings;
  }
#else
  if (pointInRect(x, y, UI_LEFT, 105, 49, 36)) {
    if (stationCount > 0) {
      selectStation((selectedStation + stationCount - 1) % stationCount);
    }
  } else if (pointInRect(x, y, UI_LEFT + 55, 105, 100, 36)) {
    if (streamActive) {
      stopRadioSong();
      streamActive = false;
      setStatus("Paused");
    } else {
      startStream();
    }
  } else if (pointInRect(x, y, UI_LEFT + 161, 105, 49, 36)) {
    if (stationCount > 0) {
      selectStation((selectedStation + 1) % stationCount);
    }
  } else if (pointInRect(x, y, UI_LEFT - 1, 163, UI_WIDTH + 2, 19)) {
    adjustVolumeFromTouch(x);
  } else if (pointInRect(x, y, UI_LEFT, HOME_BOTTOM_BUTTON_Y,
                         HOME_BOTTOM_BUTTON_WIDTH,
                         HOME_BOTTOM_BUTTON_HEIGHT)) {
    pressTimerButton(millis());
  } else if (pointInRect(x, y,
                         UI_LEFT + HOME_BOTTOM_BUTTON_WIDTH +
                             HOME_BOTTOM_BUTTON_GAP,
                         HOME_BOTTOM_BUTTON_Y, HOME_BOTTOM_BUTTON_WIDTH,
                         HOME_BOTTOM_BUTTON_HEIGHT)) {
    currentScreen = Screen::Settings;
  }
#endif
  markUiDirty();
}

// Handle station selection, paging, editing, deletion, and list navigation.
void handleStationTouch(int16_t x, int16_t y) {
#if defined(BOARD_JC3248W535)
  constexpr int16_t ROW_START_Y = 50;
  constexpr int16_t ROW_STEP = 49;
  constexpr int16_t ROW_HEIGHT = 43;
  constexpr int16_t NAME_WIDTH = 260;
  constexpr int16_t EDIT_X = 268;
  constexpr int16_t EDIT_WIDTH = 80;
  constexpr int16_t DELETE_X = 356;
  constexpr int16_t DELETE_WIDTH = 100;
  constexpr int16_t PAGER_Y = 263;
  constexpr int16_t PAGER_HEIGHT = 42;
  if (deleteConfirmation) {
    const int16_t actionWidth = (UI_WIDTH - 32) / 2;
    if (pointInRect(x, y, UI_LEFT + 8, 184, actionWidth, 48)) {
      deleteConfirmation = false;
    } else if (pointInRect(x, y, UI_LEFT + 24 + actionWidth, 184,
                           actionWidth, 48)) {
      deleteStation(deleteIndex);
    }
    markUiDirty();
    return;
  }

  const uint8_t pageStart = stationListPage * STATIONS_PER_PAGE;
  const uint8_t visibleStations =
      min<uint8_t>(STATIONS_PER_PAGE, stationCount - pageStart);
  if (pointOnMenuBackButton(x, y)) {
    currentScreen = stationListFromSettings ? Screen::Settings : Screen::Home;
  } else {
    bool rowHandled = false;
    for (uint8_t row = 0; row < visibleStations; ++row) {
      const int16_t rowY = ROW_START_Y + row * ROW_STEP;
      if (!pointInRect(x, y, UI_LEFT, rowY, UI_WIDTH, ROW_HEIGHT)) {
        continue;
      }
      rowHandled = true;
      const uint8_t index = pageStart + row;
      if (pointInRect(x, y, UI_LEFT + DELETE_X, rowY, DELETE_WIDTH,
                      ROW_HEIGHT)) {
        deleteIndex = index;
        deleteConfirmation = true;
      } else if (pointInRect(x, y, UI_LEFT + EDIT_X, rowY, EDIT_WIDTH,
                             ROW_HEIGHT)) {
        startStationEditor(false, index);
        return;
      } else if (pointInRect(x, y, UI_LEFT, rowY, NAME_WIDTH, ROW_HEIGHT)) {
        selectStation(index);
        return;
      }
      break;
    }
    if (!rowHandled) {
      if (pointInRect(x, y, UI_LEFT, PAGER_Y, 60, PAGER_HEIGHT)) {
        if (stationListPage > 0) {
          --stationListPage;
        }
      } else if (pointInRect(x, y, UI_LEFT + 70, PAGER_Y, 60,
                             PAGER_HEIGHT)) {
        if (stationListPage + 1 < stationPageCount()) {
          ++stationListPage;
        }
      } else if (pointInRect(x, y, UI_LEFT + 300, PAGER_Y, UI_WIDTH - 300,
                             PAGER_HEIGHT)) {
        if (stationCount >= MAX_STATIONS) {
          setStatus("Station list is full");
        } else {
          startStationEditor(true, stationCount);
          return;
        }
      }
    }
  }
  markUiDirty();
#else
  if (deleteConfirmation) {
    if (pointInRect(x, y, UI_LEFT + 14, 136, 127, 28)) {
      deleteConfirmation = false;
    } else if (pointInRect(x, y, UI_LEFT + 147, 136, 127, 28)) {
      deleteStation(deleteIndex);
    }
    markUiDirty();
    return;
  }

  const uint8_t pageStart = stationListPage * STATIONS_PER_PAGE;
  const uint8_t visibleStations =
      min<uint8_t>(STATIONS_PER_PAGE, stationCount - pageStart);
  if (pointOnMenuBackButton(x, y)) {
    currentScreen = stationListFromSettings ? Screen::Settings : Screen::Home;
  } else if (y >= 44 && y < 44 + visibleStations * 36) {
    const uint8_t row = (y - 44) / 36;
    const uint8_t index = stationListPage * STATIONS_PER_PAGE + row;
    const int16_t rowY = 44 + row * 36;
    if (row < STATIONS_PER_PAGE && index < stationCount &&
        pointInRect(x, y, UI_LEFT, rowY, UI_WIDTH, 32)) {
      if (x >= UI_LEFT + 242 && x < UI_LEFT + 296) {
        deleteIndex = index;
        deleteConfirmation = true;
      } else if (x >= UI_LEFT + 184 && x < UI_LEFT + 236) {
        startStationEditor(false, index);
        return;
      } else if (x >= UI_LEFT && x < UI_LEFT + 178) {
        selectStation(index);
        return;
      }
    }
  } else {
    if (pointInRect(x, y, UI_LEFT, 197, 40, 36)) {
      if (stationListPage > 0) {
        --stationListPage;
      }
    } else if (pointInRect(x, y, UI_LEFT + 46, 197, 40, 36)) {
      if (stationListPage + 1 < stationPageCount()) {
        ++stationListPage;
      }
    } else if (pointInRect(x, y, UI_LEFT + 164, 197, 132, 36)) {
      if (stationCount >= MAX_STATIONS) {
        setStatus("Station list is full");
      } else {
        startStationEditor(true, stationCount);
        return;
      }
    }
  }
  markUiDirty();
#endif
}

// Dispatch a calibrated touch to the active screen and its controls.
void handleTouch(int16_t x, int16_t y) {
  updateScreenLayout();
  switch (currentScreen) {
    case Screen::Home:
      if (timerState == TimerState::Expired &&
          pointInRect(x, y, 0, 0,
#if defined(BOARD_JC3248W535)
                      CAT_PANEL_WIDTH, DISPLAY_HEIGHT
#else
                      DISPLAY_WIDTH / 2, DISPLAY_HEIGHT
#endif
                      )) {
        pressTimerButton(millis());
        return;
      }
      if (pointInRect(x, y, 0,
#if defined(BOARD_JC3248W535)
                      265, CAT_PANEL_WIDTH, 55
#else
                      195, CAT_PANEL_WIDTH, 45
#endif
                      )) {
        setAudioSpectrum(!audioSpectrumEnabled);
        return;
      }
      if (pointInRect(x, y, UI_LEFT, HOME_BOTTOM_BUTTON_Y,
                      HOME_BOTTOM_BUTTON_WIDTH,
                      HOME_BOTTOM_BUTTON_HEIGHT)) {
        pressTimerButton(millis());
        return;
      }
      if (pointInRect(x, y, 0, 0, CAT_PANEL_WIDTH,
#if defined(BOARD_JC3248W535)
                      195
#else
                      195
#endif
                      )) {
        if (timerModeActive()) {
          handleTimerTouch(x, y, millis());
        } else {
          chooseDifferentPet();
        }
        return;
      }
      handleHomeTouch(x, y);
      break;
    case Screen::Settings:
    {
      if (pointOnMenuBackButton(x, y)) {
        currentScreen = Screen::Home;
      } else if (x >= UI_LEFT && x < UI_LEFT + UI_WIDTH &&
                 y >= SETTINGS_ITEM_START_Y) {
        const int16_t rowStep = SETTINGS_ITEM_HEIGHT + SETTINGS_ITEM_GAP;
        const uint8_t row =
            static_cast<uint8_t>((y - SETTINGS_ITEM_START_Y) / rowStep);
        const int16_t rowY = SETTINGS_ITEM_START_Y + row * rowStep;
        if (pointInRect(x, y, UI_LEFT, rowY, UI_WIDTH,
                        SETTINGS_ITEM_HEIGHT)) {
          switch (row) {
            case 0:
              currentScreen = Screen::AudioSettings;
              break;
            case 1:
              currentScreen = Screen::DisplaySettings;
              break;
            case 2:
              currentScreen = Screen::Wifi;
              break;
            case 3:
              currentScreen = Screen::Stations;
              deleteConfirmation = false;
              stationListFromSettings = true;
              break;
#if defined(BOARD_FREENOVE_FNK0114L)
            case 4:
              currentScreen = Screen::RgbLedSettings;
              break;
#endif
          }
        }
      }
      markUiDirty();
      break;
    }
    case Screen::AudioSettings:
      if (pointOnMenuBackButton(x, y)) {
        currentScreen = Screen::Settings;
      } else if (pointInRect(x, y, UI_LEFT, AUDIO_TOGGLE_START_Y, UI_WIDTH,
                             AUDIO_TOGGLE_HEIGHT)) {
        setAudioBoost(!audioBoost);
      } else if (pointInRect(x, y, UI_LEFT,
                             AUDIO_TOGGLE_START_Y + AUDIO_TOGGLE_STEP,
                             UI_WIDTH, AUDIO_TOGGLE_HEIGHT)) {
        setAudioSpectrum(!audioSpectrumEnabled);
      } else if (pointInRect(x, y, UI_LEFT,
                             AUDIO_TOGGLE_START_Y + 2 * AUDIO_TOGGLE_STEP,
                             UI_WIDTH, AUDIO_TOGGLE_HEIGHT)) {
        setAudioCompressor(!audioCompressorEnabled);
#if defined(BOARD_JC3248W535)
      } else if (pointInRect(x, y, UI_LEFT, AUDIO_VOLUME_TRACK_Y - 8,
                             UI_WIDTH, 30)) {
        adjustVolumeFromTouch(x);
#else
      } else if (pointInRect(x, y, UI_LEFT, 133, UI_WIDTH, 29)) {
        setAudioAmpEnabled(!audioAmpEnabled);
      } else if (pointInRect(x, y, UI_LEFT, 165, UI_WIDTH, 29)) {
        setAudioNoiseShaping(!audioNoiseShaping);
      } else if (pointInRect(x, y, UI_LEFT, 190, UI_WIDTH, 50)) {
        adjustVolumeFromTouch(x);
#endif
      }
      markUiDirty();
      break;
    case Screen::DisplaySettings: {
      if (pointOnMenuBackButton(x, y)) {
        currentScreen = Screen::Settings;
#if defined(BOARD_JC3248W535)
      } else if (pointInRect(x, y, UI_LEFT, 60, UI_WIDTH, 72)) {
        constexpr int16_t BACKGROUND_SWATCH_Y = 62;
        constexpr int16_t SWATCH_ROW_STEP = 34;
#else
      } else if (pointInRect(x, y, UI_LEFT, 51, UI_WIDTH, 56)) {
        constexpr int16_t BACKGROUND_SWATCH_Y = 53;
        constexpr int16_t SWATCH_ROW_STEP = 29;
#endif
        for (uint8_t index = 0; index < THEME_COLOR_COUNT; ++index) {
          const int16_t swatchY =
              BACKGROUND_SWATCH_Y + (index / COLORS_PER_ROW) * SWATCH_ROW_STEP;
          if (pointInRect(x, y, themeSwatchX(index % COLORS_PER_ROW) - 2,
                          swatchY - 2, themeSwatchWidth + 4, 28)) {
            setThemeColor(index);
            break;
          }
        }
#if defined(BOARD_JC3248W535)
      } else if (pointInRect(x, y, UI_LEFT, 160, UI_WIDTH, 72)) {
        constexpr int16_t BUTTON_SWATCH_Y = 162;
        constexpr int16_t SWATCH_ROW_STEP = 34;
#else
      } else if (pointInRect(x, y, UI_LEFT, 126, UI_WIDTH, 57)) {
        constexpr int16_t BUTTON_SWATCH_Y = 128;
        constexpr int16_t SWATCH_ROW_STEP = 29;
#endif
        for (uint8_t index = 0; index < BUTTON_COLOR_COUNT; ++index) {
          const int16_t swatchY =
              BUTTON_SWATCH_Y + (index / COLORS_PER_ROW) * SWATCH_ROW_STEP;
          if (pointInRect(x, y, themeSwatchX(index % COLORS_PER_ROW) - 2,
                          swatchY - 2, themeSwatchWidth + 4, 28)) {
            setButtonColor(index);
            break;
          }
        }
#if defined(BOARD_JC3248W535)
      } else if (pointInRect(x, y, UI_LEFT, 238, UI_WIDTH, 28)) {
        adjustDisplayBrightnessFromTouch(x);
      } else if (pointInRect(x, y, UI_LEFT, 270, UI_WIDTH, 38)) {
#else
      } else if (pointInRect(x, y, UI_LEFT, 194, UI_WIDTH, 36)) {
#endif
        calibrateTouchscreen();
      }
      markUiDirty();
      break;
    }
#if defined(BOARD_FREENOVE_FNK0114L)
    case Screen::RgbLedSettings: {
      if (pointOnMenuBackButton(x, y)) {
        currentScreen = Screen::Settings;
      } else if (pointInRect(x, y, UI_LEFT, 48, UI_WIDTH, 32)) {
        setRgbLedEnabled(!rgbLedEnabled);
      } else if (pointInRect(x, y, UI_LEFT, 88, UI_WIDTH, 32)) {
        setRgbLedPulseEnabled(!rgbLedPulseEnabled);
      } else if (pointInRect(x, y, UI_LEFT, 149, UI_WIDTH, 40)) {
        constexpr int16_t COLOR_BUTTON_WIDTH = 52;
        constexpr int16_t COLOR_BUTTON_GAP = 7;
        const int16_t colorsWidth =
            RGB_LED_COLOR_COUNT * COLOR_BUTTON_WIDTH +
            (RGB_LED_COLOR_COUNT - 1) * COLOR_BUTTON_GAP;
        const int16_t colorsLeft = UI_LEFT + (UI_WIDTH - colorsWidth) / 2;
        for (uint8_t index = 0; index < RGB_LED_COLOR_COUNT; ++index) {
          const int16_t colorX =
              colorsLeft + index * (COLOR_BUTTON_WIDTH + COLOR_BUTTON_GAP);
          if (pointInRect(x, y, colorX - 1, 149,
                          COLOR_BUTTON_WIDTH + 2, 40)) {
            setRgbLedColor(index);
            break;
          }
        }
      }
      markUiDirty();
      break;
    }
#endif
    case Screen::Wifi:
      if (pointOnMenuBackButton(x, y)) {
        currentScreen = Screen::Settings;
        markUiDirty();
#if defined(BOARD_JC3248W535)
      } else if (pointInRect(x, y, UI_LEFT, 62, UI_WIDTH, 48)) {
        beginKeyboard(KeyboardTarget::Ssid);
      } else if (pointInRect(x, y, UI_LEFT, 146, UI_WIDTH, 48)) {
        beginKeyboard(KeyboardTarget::Password);
      } else if (pointInRect(x, y, UI_LEFT, 202, UI_WIDTH, 44)) {
        showWifiPassword = !showWifiPassword;
        markUiDirty();
      } else if (pointInRect(x, y, UI_LEFT, 254, UI_WIDTH, 44)) {
#else
      } else if (pointInRect(x, y, UI_LEFT, 55, UI_WIDTH, 39)) {
        beginKeyboard(KeyboardTarget::Ssid);
      } else if (pointInRect(x, y, UI_LEFT, 118, UI_WIDTH, 39)) {
        beginKeyboard(KeyboardTarget::Password);
      } else if (pointInRect(x, y, UI_LEFT, 157, UI_WIDTH, 38)) {
        showWifiPassword = !showWifiPassword;
        markUiDirty();
      } else if (pointInRect(x, y, UI_LEFT, 198, UI_WIDTH, 32)) {
#endif
        if (saveWifiCredentials()) {
          beginWifiConnection();
        }
      }
      break;
    case Screen::Stations:
      handleStationTouch(x, y);
      break;
    case Screen::StationEditor:
#if defined(BOARD_JC3248W535)
      if (pointInRect(x, y, UI_LEFT, 64, UI_WIDTH, 48)) {
        beginKeyboard(KeyboardTarget::StationName);
      } else if (pointInRect(x, y, UI_LEFT, 148, UI_WIDTH, 48)) {
        beginKeyboard(KeyboardTarget::StationUrl);
      } else if (pointInRect(x, y, UI_LEFT, 220, (UI_WIDTH - 8) / 2, 48)) {
        saveStationEditor();
      } else if (pointInRect(x, y, UI_LEFT + (UI_WIDTH - 8) / 2 + 8, 220,
                             (UI_WIDTH - 8) / 2, 48)) {
        currentScreen = Screen::Stations;
        markUiDirty();
      }
#else
      if (pointInRect(x, y, UI_LEFT, 57, UI_WIDTH, 39)) {
        beginKeyboard(KeyboardTarget::StationName);
      } else if (pointInRect(x, y, UI_LEFT, 122, UI_WIDTH, 39)) {
        beginKeyboard(KeyboardTarget::StationUrl);
      } else if (pointInRect(x, y, UI_LEFT, 181, (UI_WIDTH - 8) / 2, 40)) {
        saveStationEditor();
      } else if (pointInRect(x, y, UI_LEFT + (UI_WIDTH - 8) / 2 + 8, 181,
                             (UI_WIDTH - 8) / 2, 40)) {
        currentScreen = Screen::Stations;
        markUiDirty();
      }
#endif
      break;
    case Screen::Keyboard:
      handleKeyboardTouch(x, y);
      break;
  }
}

}  // namespace

void audio_info(const char *) {}

// Audio-library callback for the station name currently being decoded.
void audio_showstation(const char *info) {
#if defined(BOARD_JC3248W535)
  portENTER_CRITICAL(&audioCallbackMux);
  strlcpy(pendingStationName, info, sizeof(pendingStationName));
  pendingStationNameUpdate = true;
  portEXIT_CRITICAL(&audioCallbackMux);
#else
  streamStation = info;
  audioDirty = true;
#endif
}

// Audio-library callback for the current stream title.
void audio_showstreamtitle(const char *info) {
#if defined(BOARD_JC3248W535)
  portENTER_CRITICAL(&audioCallbackMux);
  strlcpy(pendingStreamTitle, info, sizeof(pendingStreamTitle));
  pendingStreamTitleUpdate = true;
  portEXIT_CRITICAL(&audioCallbackMux);
#else
  streamTitle = info;
  audioDirty = true;
#endif
}

// Schedule a delayed reconnect when the decoder reports end of stream.
void audio_eof_stream(const char *info) {
#if defined(BOARD_JC3248W535)
  (void)info;
  portENTER_CRITICAL(&audioCallbackMux);
  pendingStreamEndUpdate = true;
  portEXIT_CRITICAL(&audioCallbackMux);
#else
  (void)info;
  if (!streamActive) return;
  streamActive = false;
  streamRestartPending = true;
  streamRestartAt = millis();
  setStatus("Stream ended");
#endif
}

// Initialize hardware, restore settings, calibrate touch, and start networking.
void setup() {
  Serial.begin(115200);
#if defined(BOARD_JC3248W535)
  if (!display.init()) {
    Serial.println("ERROR: JC3248W535 display initialization failed.");
    while (true) {
      delay(1000);
    }
  }
#else
  display.init();
#endif
  display.setRotation(1);
  display.fillScreen(TFT_BLACK);
#if defined(BOARD_JC3248W535)
  display.flushFrame();
#endif
#if !defined(CYD_LOW_MEMORY_DIRECT_RENDER)
  screenFrameBuffer.setColorDepth(8);
  if (!screenFrameBuffer.createSprite(DISPLAY_WIDTH, DISPLAY_HEIGHT)) {
    Serial.printf("ERROR: could not allocate %u-byte screen frame buffer; "
                  "%u bytes of heap remain.\n",
                  static_cast<unsigned>(DISPLAY_WIDTH * DISPLAY_HEIGHT),
                  static_cast<unsigned>(ESP.getFreeHeap()));
    display.fillScreen(TFT_RED);
    display.setTextColor(TFT_WHITE, TFT_RED);
    display.drawCentreString("Frame buffer unavailable", DISPLAY_WIDTH / 2,
                             DISPLAY_HEIGHT / 2, 2);
    while (true) {
      delay(1000);
    }
  }
  displayFramebufferReady = true;
#elif !defined(BOARD_JC3248W535)
  petPanelFrameBuffer.setColorDepth(8);
  petPanelFramebufferReady =
      petPanelFrameBuffer.createSprite(CAT_PANEL_WIDTH, DISPLAY_HEIGHT);
  if (!petPanelFramebufferReady) {
    Serial.printf("ERROR: could not allocate %u-byte pet panel framebuffer.\n",
                  static_cast<unsigned>(CAT_PANEL_WIDTH * DISPLAY_HEIGHT));
  } else {
    petPanelFrameBuffer.fillSprite(0x0841);
  }
  homeUpdateFrameBuffer.setColorDepth(8);
  homeUpdateFramebufferReady =
      homeUpdateFrameBuffer.createSprite(HOME_UI_WIDTH, 62);
  if (!homeUpdateFramebufferReady) {
    Serial.printf("WARNING: could not allocate %u-byte home update framebuffer; "
                  "partial updates will draw directly.\n",
                  static_cast<unsigned>(HOME_UI_WIDTH * 62));
  }
#endif

#if defined(BOARD_JC3248W535)
  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
  Wire.setClock(400000);
  pinMode(TOUCH_RESET_PIN, OUTPUT);
  digitalWrite(TOUCH_RESET_PIN, LOW);
  delay(20);
  digitalWrite(TOUCH_RESET_PIN, HIGH);
  delay(200);
#elif !defined(BOARD_FREENOVE_FNK0114L)
  touchSPI.begin(25, 39, 32, 33);
  if (!touch.begin(touchSPI)) {
    Serial.println("ERROR: XPT2046 touch controller initialization failed.");
  }
  touch.setRotation(1);
#endif

  if (!preferences.begin("pet-radio", false)) {
    Serial.println("ERROR: could not open preferences storage.");
  }
  wifiSsid = preferences.getString("ssid", "");
  wifiPassword = preferences.getString("password", "");
  volume = min<uint8_t>(preferences.getUChar("volume", 10), 21);
#if defined(BOARD_JC3248W535)
  displayBrightness =
      constrain(preferences.getUChar("brightness", DISPLAY_BRIGHTNESS_DEFAULT),
                DISPLAY_BRIGHTNESS_MIN, DISPLAY_BRIGHTNESS_MAX);
  ledcSetup(DISPLAY_BACKLIGHT_CHANNEL, 5000, 8);
  ledcAttachPin(DISPLAY_BACKLIGHT_PIN, DISPLAY_BACKLIGHT_CHANNEL);
  applyDisplayBrightness(displayBrightness);
#endif
  audioBoost = preferences.getBool("boost", false);
#if !defined(BOARD_JC3248W535)
  audioAmpEnabled = preferences.getBool("amp4", false);
#endif
  audioSpectrumEnabled = preferences.getBool("spectrum", true);
  audioCompressorEnabled = preferences.getBool("compressor", false);
  setAudioCompressorEnabled(audioCompressorEnabled);
#if defined(BOARD_JC3248W535)
  audioNoiseShaping = false;
  setAudioNoiseShapingEnabled(false);
#else
  audioNoiseShaping = preferences.getBool("noiseshape", false);
  setAudioNoiseShapingEnabled(audioNoiseShaping);
  digitalWrite(AUDIO_AMP_ENABLE_PIN, audioAmpEnabled ? LOW : HIGH);
  pinMode(AUDIO_AMP_ENABLE_PIN, OUTPUT);
#endif
  themeColorIndex = min<uint8_t>(
      preferences.getUChar("theme", DEFAULT_THEME_COLOR),
      THEME_COLOR_COUNT - 1);
  backgroundColor = THEME_COLORS[themeColorIndex];
  buttonColorIndex = min<uint8_t>(
      preferences.getUChar("buttonColor", DEFAULT_BUTTON_COLOR),
      BUTTON_COLOR_COUNT - 1);
  timerDurationSeconds = min<uint32_t>(
      preferences.getUInt("timerSecs", DEFAULT_TIMER_SECONDS),
      MAX_TIMER_SECONDS);
#if defined(BOARD_FREENOVE_FNK0114L)
  rgbLedEnabled = preferences.getBool("rgbEnabled", false);
  rgbLedPulseEnabled = preferences.getBool("rgbPulse", false);
  rgbLedColorIndex = min<uint8_t>(preferences.getUChar("rgbColor", 0),
                                  RGB_LED_COLOR_COUNT - 1);
  initializeRgbLed();
#endif
  display.fillScreen(backgroundColor);
  setAudioSpectrumEnabled(audioSpectrumEnabled);
  selectedStation = preferences.getUChar("selected", 0);
  loadStations();
  if (selectedStation >= stationCount) {
    selectedStation = 0;
  }

#if defined(BOARD_FREENOVE_FNK0114L)
  if (preferences.getUChar("ftVer", 0) == TOUCH_CALIBRATION_VERSION) {
    freenoveTouchCalibration.xRawX = preferences.getFloat("ftXx", 0);
    freenoveTouchCalibration.xRawY = preferences.getFloat("ftXy", 0);
    freenoveTouchCalibration.xOffset = preferences.getFloat("ftXo", 0);
    freenoveTouchCalibration.yRawX = preferences.getFloat("ftYx", 0);
    freenoveTouchCalibration.yRawY = preferences.getFloat("ftYy", 0);
    freenoveTouchCalibration.yOffset = preferences.getFloat("ftYo", 0);
    freenoveTouchCalibration.valid =
        isTouchCalibrationValid(freenoveTouchCalibration);
  }
  if (!freenoveTouchCalibration.valid) {
    calibrateTouchscreen();
  }
#else
  if (preferences.getUChar("touchVer", 0) == TOUCH_CALIBRATION_VERSION) {
    touchCalibration.xRawX = preferences.getFloat("touchXx", 0);
    touchCalibration.xRawY = preferences.getFloat("touchXy", 0);
    touchCalibration.xOffset = preferences.getFloat("touchXo", 0);
    touchCalibration.yRawX = preferences.getFloat("touchYx", 0);
    touchCalibration.yRawY = preferences.getFloat("touchYy", 0);
    touchCalibration.yOffset = preferences.getFloat("touchYo", 0);
    touchCalibration.valid =
        isfinite(touchCalibration.xRawX) &&
        isfinite(touchCalibration.xRawY) &&
        isfinite(touchCalibration.xOffset) &&
        isfinite(touchCalibration.yRawX) &&
        isfinite(touchCalibration.yRawY) &&
        isfinite(touchCalibration.yOffset) &&
        (fabsf(touchCalibration.xRawX) +
         fabsf(touchCalibration.xRawY) > 1e-6f) &&
        (fabsf(touchCalibration.yRawX) +
         fabsf(touchCalibration.yRawY) > 1e-6f);
  }
  if (!touchCalibration.valid) {
    calibrateTouchscreen();
  }
#endif

  // Give the audio library enough input buffering for network jitter.
#if defined(BOARD_JC3248W535)
  radioAudio.setBufsize(AUDIO_INPUT_BUFFER_BYTES, AUDIO_PSRAM_BUFFER_BYTES);
#else
  radioAudio.setBufsize(AUDIO_INPUT_BUFFER_BYTES, 0);
#endif
  radioAudio.forceMono(true);
#if defined(BOARD_JC3248W535)
  if (!radioAudio.setPinout(42, 2, 41)) {
    Serial.println("ERROR: could not configure the board's I2S audio output.");
    setStatus("I2S audio output unavailable");
  }
#endif
  const int8_t audioGain = audioBoost ? AUDIO_BOOST_DB : 0;
  radioAudio.setTone(audioGain, audioGain, audioGain);
  radioAudio.setVolume(volume);
#if defined(BOARD_JC3248W535)
  if (!startRadioAudioTask()) {
    while (true) {
      delay(1000);
    }
  }
#endif
  WiFi.mode(WIFI_STA);
  configTzTime(BERLIN_TIMEZONE, "pool.ntp.org", "time.google.com");
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.setScanMethod(WIFI_ALL_CHANNEL_SCAN);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
      wifiDisconnectReason = info.wifi_sta_disconnected.reason;
    }
  });
  if (!wifiSsid.isEmpty()) {
    startWifiConnection();
  } else {
    statusMessage = "Set up Wi-Fi";
  }
  lastPetChangeAt = millis();
  markUiDirty();
}

// Service audio, networking, timer, animation, and display work continuously.
void loop() {
#if defined(BOARD_JC3248W535)
  applyPendingAudioCallbacks();
#endif
  updateNetwork();

  int16_t x = 0;
  int16_t y = 0;
  if (readTouchPoint(x, y)) {
    handleTouch(x, y);
  }

  const uint32_t now = millis();
#if defined(BOARD_FREENOVE_FNK0114L)
  updateRgbLed(now);
#endif
  updateTimer(now);
  if (streamRestartPending && WiFi.status() == WL_CONNECTED &&
      stationCount > 0 &&
      now - streamRestartAt >= STREAM_RETRY_DELAY_MS) {
    startStream();
  }
  if (!timerModeActive()) {
    if (petTransition == PetTransition::Idle &&
        displayedPetIndex == selectedPetIndex &&
        now - lastPetChangeAt >= RANDOM_PET_INTERVAL_MS) {
      chooseDifferentPet();
    }
    updatePetAnimation(now);
    updateBubbleText(now);
  }
  if (currentScreen == Screen::Home &&
      now - lastClockUpdateAt >= CLOCK_UPDATE_INTERVAL_MS) {
    lastClockUpdateAt = now;
    homeClockDirty = true;
  }
  if (currentScreen == Screen::Home &&
      now - lastBitrateUpdateAt >= BITRATE_UPDATE_INTERVAL_MS) {
    lastBitrateUpdateAt = now;
    const uint32_t bitrateKbps =
        WiFi.status() == WL_CONNECTED && streamActive
            ? (radioBitRate() + 500) / 1000
            : 0;
    const int codecId =
        WiFi.status() == WL_CONNECTED && streamActive
            ? radioCodec()
            : 0;
    if (bitrateKbps != lastDisplayedBitrateKbps ||
        codecId != lastDisplayedCodec) {
      lastDisplayedBitrateKbps = bitrateKbps;
      lastDisplayedCodec = codecId;
      homeBitrateDirty = true;
    }
  }
  if (audioDirty) {
    audioDirty = false;
    if (currentScreen == Screen::Home) {
      homeMetadataDirty = true;
    }
  }
  if (currentScreen != Screen::Home && uiDirty) {
    renderScreen();
  }
  if (currentScreen == Screen::Home &&
      now - lastCatFrameAt >= CAT_FRAME_INTERVAL_MS) {
    lastCatFrameAt = now;
    if (uiDirty) {
      renderScreen();
    } else {
      if (homeMetadataDirty) {
        refreshHomeMetadata();
      } else if (homeBitrateDirty) {
        refreshHomeBitrate();
      }
      if (homeClockDirty) {
        refreshHomeClock();
      }
      drawCatFrame(now);
    }
  }
#if defined(BOARD_JC3248W535)
  display.flushFrame();
#endif
  delay(1);
}
