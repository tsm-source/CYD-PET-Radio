#if defined(BOARD_FREENOVE_FNK0104S)
#include "sd_music.h"
#include <cstring>
#include <driver/sdmmc_host.h>
#include <sdmmc_cmd.h>
#include <diskio_impl.h>
#include <diskio_sdmmc.h>
#include <ff.h>

namespace sdMusic {
namespace {
bool mounted = false;

bool isMp3(const char *name) {
  const size_t length = strlen(name);
  return length > 4 && strcasecmp(name + length - 4, ".mp3") == 0;
}

bool makePath(const char *directory, const char *name, char *path,
              size_t pathSize) {
  const int length = snprintf(path, pathSize, "%s%s%s", directory,
                              strcmp(directory, "/") == 0 ? "" : "/", name);
  return length >= 0 && static_cast<size_t>(length) < pathSize;
}
}

bool mount() {
  if (mounted) return true;
  if (!SD_MMC.setPins(38, 40, 39, 41, 48, 47)) {
    Serial.println("ERROR: SD-MMC pin configuration failed.");
    return false;
  }
  if (!SD_MMC.begin("/sdcard", false, false)) {
    Serial.println("ERROR: SD card mount failed; FAT32 card required.");
    return false;
  }
  mounted = true;
  return true;
}

void unmount() {
  if (mounted) SD_MMC.end();
  mounted = false;
}

bool readPage(const char *path, uint32_t page, Entry *entries,
              uint8_t &count, bool &hasNext) {
  count = 0;
  hasNext = false;
  File directory = SD_MMC.open(path);
  if (!directory || !directory.isDirectory()) {
    Serial.printf("ERROR: could not open SD directory: %s\n", path);
    return false;
  }
  uint32_t index = 0;
  File file = directory.openNextFile();
  while (file) {
    const char *name = file.name();
    const bool isDirectory = file.isDirectory();
    if (name[0] != '.' && (isDirectory || isMp3(name))) {
      if (index >= page * PAGE_SIZE && count < PAGE_SIZE) {
        strlcpy(entries[count].name, name, sizeof(entries[count].name));
        entries[count++].directory = isDirectory;
      } else if (index >= (page + 1) * PAGE_SIZE) {
        hasNext = true;
        file.close();
        break;
      }
      ++index;
    }
    file.close();
    file = directory.openNextFile();
  }
  directory.close();
  return true;
}

bool adjacentTrack(const char *directoryPath, const char *current, bool next,
                   char *path, size_t pathSize) {
  File directory = SD_MMC.open(directoryPath);
  if (!directory || !directory.isDirectory()) {
    Serial.printf("ERROR: could not open SD directory: %s\n", directoryPath);
    return false;
  }
  char first[PATH_SIZE] = "";
  char previous[PATH_SIZE] = "";
  bool found = current[0] == '\0';
  File file = directory.openNextFile();
  while (file) {
    if (!file.isDirectory() && file.name()[0] != '.' && isMp3(file.name())) {
      char candidate[PATH_SIZE];
      if (!makePath(directoryPath, file.name(), candidate, sizeof(candidate))) {
        Serial.println("ERROR: MP3 path exceeds 255 bytes.");
      } else {
        if (first[0] == '\0') strlcpy(first, candidate, sizeof(first));
        if (found && next) {
          strlcpy(path, candidate, pathSize);
          file.close();
          directory.close();
          return true;
        }
        if (strcmp(candidate, current) == 0) {
          if (!next && previous[0] != '\0') {
            strlcpy(path, previous, pathSize);
            file.close();
            directory.close();
            return true;
          }
          found = true;
        }
        strlcpy(previous, candidate, sizeof(previous));
      }
    }
    file.close();
    file = directory.openNextFile();
  }
  directory.close();
  if (!found || first[0] == '\0') return false;
  strlcpy(path, next ? first : previous, pathSize);
  return true;
}

bool formatFat32() {
  unmount();
  sdmmc_host_t host = SDMMC_HOST_DEFAULT();
  host.slot = SDMMC_HOST_SLOT_1;
  host.flags = SDMMC_HOST_FLAG_4BIT;
  sdmmc_slot_config_t slot = SDMMC_SLOT_CONFIG_DEFAULT();
  slot.clk = GPIO_NUM_38;
  slot.cmd = GPIO_NUM_40;
  slot.d0 = GPIO_NUM_39;
  slot.d1 = GPIO_NUM_41;
  slot.d2 = GPIO_NUM_48;
  slot.d3 = GPIO_NUM_47;
  slot.width = 4;
  esp_err_t result = host.init();
  if (result != ESP_OK) {
    Serial.printf("ERROR: SD host init failed: %d\n", result);
    return false;
  }
  result = sdmmc_host_init_slot(host.slot, &slot);
  sdmmc_card_t card{};
  if (result == ESP_OK) result = sdmmc_card_init(&host, &card);
  if (result != ESP_OK) {
    Serial.printf("ERROR: SD card init failed: %d\n", result);
    host.deinit();
    return false;
  }
  BYTE drive = FF_DRV_NOT_USED;
  if (ff_diskio_get_drive(&drive) != ESP_OK || drive == FF_DRV_NOT_USED) {
    Serial.println("ERROR: no free FAT drive for SD formatting.");
    host.deinit();
    return false;
  }
  ff_diskio_register_sdmmc(drive, &card);
  char driveName[] = {static_cast<char>('0' + drive), ':', '\0'};
  constexpr size_t WORK_SIZE = 4096;
  auto *work = static_cast<BYTE *>(malloc(WORK_SIZE));
  if (work == nullptr) {
    Serial.println("ERROR: could not allocate SD format buffer.");
    ff_diskio_unregister(drive);
    host.deinit();
    return false;
  }
  DWORD partitions[] = {100, 0, 0, 0};
  FRESULT fatResult = f_fdisk(drive, partitions, work);
  if (fatResult == FR_OK) {
    fatResult = f_mkfs(driveName, FM_FAT32, 32768, work, WORK_SIZE);
  }
  free(work);
  ff_diskio_unregister(drive);
  result = host.deinit();
  if (fatResult != FR_OK || result != ESP_OK) {
    Serial.printf("ERROR: SD FAT32 format failed: FAT=%d host=%d\n",
                  static_cast<int>(fatResult), result);
    return false;
  }
  if (!mount()) {
    Serial.println("ERROR: formatted SD card could not be mounted.");
    return false;
  }
  return true;
}
}
#endif
