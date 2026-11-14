#if defined(BOARD_FREENOVE_FNK0104S)
#include "sd_usb.h"

#include <Arduino.h>
#include <USBMSC.h>
#include <atomic>
#include <driver/sdmmc_host.h>
#include <esp_attr.h>
#include <sdmmc_cmd.h>

namespace {
USBMSC storage;
sdmmc_host_t host = SDMMC_HOST_DEFAULT();
sdmmc_card_t card{};
SemaphoreHandle_t cardMutex = nullptr;
bool active = false;
std::atomic<bool> ejected{false};
constexpr size_t SECTOR_SIZE = 512;
constexpr size_t TRANSFER_SIZE = 4096;
DRAM_ATTR alignas(4) uint8_t transferBuffer[TRANSFER_SIZE];

int32_t transfer(uint32_t lba, uint32_t offset, void *buffer, uint32_t size,
                 bool write) {
  if (cardMutex == nullptr ||
      xSemaphoreTake(cardMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("ERROR: USB SD card lock failed.");
    return -1;
  }
  if (!active || ejected.load() || offset >= SECTOR_SIZE ||
      static_cast<uint64_t>(lba) * SECTOR_SIZE + offset + size >
          static_cast<uint64_t>(card.csd.capacity) * SECTOR_SIZE) {
    Serial.println("ERROR: invalid or unavailable USB SD transfer.");
    xSemaphoreGive(cardMutex);
    return -1;
  }
  uint8_t *bytes = static_cast<uint8_t *>(buffer);
  uint32_t remaining = size;
  esp_err_t result = ESP_OK;
  while (remaining > 0) {
    if (offset == 0 && remaining >= SECTOR_SIZE) {
      const uint32_t blocks =
          min<uint32_t>(remaining / SECTOR_SIZE, TRANSFER_SIZE / SECTOR_SIZE);
      const uint32_t count = blocks * SECTOR_SIZE;
      if (write) {
        memcpy(transferBuffer, bytes, count);
        result = sdmmc_write_sectors(&card, transferBuffer, lba, blocks);
      } else {
        result = sdmmc_read_sectors(&card, transferBuffer, lba, blocks);
        if (result == ESP_OK) memcpy(bytes, transferBuffer, count);
      }
      if (result != ESP_OK) break;
      bytes += count;
      remaining -= count;
      lba += blocks;
      continue;
    }
    const uint32_t count = min<uint32_t>(remaining, SECTOR_SIZE - offset);
    result = sdmmc_read_sectors(&card, transferBuffer, lba, 1);
    if (result != ESP_OK) break;
    if (write) {
      memcpy(transferBuffer + offset, bytes, count);
      result = sdmmc_write_sectors(&card, transferBuffer, lba, 1);
      if (result != ESP_OK) break;
    } else {
      memcpy(bytes, transferBuffer + offset, count);
    }
    bytes += count;
    remaining -= count;
    ++lba;
    offset = 0;
  }
  xSemaphoreGive(cardMutex);
  if (result != ESP_OK) {
    Serial.printf("ERROR: USB SD %s failed: %d\n", write ? "write" : "read",
                  result);
    return -1;
  }
  return static_cast<int32_t>(size);
}

int32_t readBlocks(uint32_t lba, uint32_t offset, void *buffer, uint32_t size) {
  return transfer(lba, offset, buffer, size, false);
}

int32_t writeBlocks(uint32_t lba, uint32_t offset, uint8_t *buffer,
                    uint32_t size) {
  return transfer(lba, offset, buffer, size, true);
}

bool startStop(uint8_t, bool start, bool loadEject) {
  if (loadEject && !start) {
    storage.mediaPresent(false);
    ejected.store(true);
  }
  return true;
}

struct ConfigureStorage {
  ConfigureStorage() {
    storage.vendorID("CYDPET");
    storage.productID("SD MUSIC");
    storage.productRevision("1.0");
    storage.onRead(readBlocks);
    storage.onWrite(writeBlocks);
    storage.onStartStop(startStop);
  }
} configureStorage;
}

namespace sdUsb {
bool start() {
  if (active) return false;
  if (cardMutex == nullptr) cardMutex = xSemaphoreCreateMutex();
  if (cardMutex == nullptr) {
    Serial.println("ERROR: could not create USB SD card lock.");
    return false;
  }
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
    Serial.printf("ERROR: USB SD host init failed: %d\n", result);
    return false;
  }
  result = sdmmc_host_init_slot(host.slot, &slot);
  if (result == ESP_OK) result = sdmmc_card_init(&host, &card);
  if (result != ESP_OK || card.csd.sector_size != SECTOR_SIZE ||
      card.csd.capacity <= 0) {
    Serial.printf("ERROR: USB SD card init failed: %d, sector size: %d\n",
                  result, card.csd.sector_size);
    host.deinit();
    return false;
  }
  if (!storage.begin(card.csd.capacity, card.csd.sector_size)) {
    Serial.println("ERROR: USB mass storage initialization failed.");
    host.deinit();
    return false;
  }
  ejected.store(false);
  active = true;
  storage.mediaPresent(true);
  return true;
}

bool hostEjected() { return ejected.load(); }

bool stop() {
  if (cardMutex == nullptr ||
      xSemaphoreTake(cardMutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
    Serial.println("ERROR: USB SD card lock failed during shutdown.");
    return false;
  }
  storage.mediaPresent(false);
  active = false;
  const esp_err_t result = host.deinit();
  xSemaphoreGive(cardMutex);
  if (result != ESP_OK) {
    Serial.printf("ERROR: USB SD host deinit failed: %d\n", result);
    return false;
  }
  return true;
}
}
#endif
