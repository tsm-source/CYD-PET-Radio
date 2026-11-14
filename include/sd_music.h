#pragma once

#if defined(BOARD_FREENOVE_FNK0104S)
#include <Arduino.h>
#include <SD_MMC.h>

namespace sdMusic {
constexpr size_t PAGE_SIZE = 4;
constexpr size_t PATH_SIZE = 256;

struct Entry {
  char name[256];
  bool directory;
};

bool mount();
void unmount();
bool readPage(const char *path, uint32_t page, Entry *entries,
              uint8_t &count, bool &hasNext);
bool adjacentTrack(const char *directory, const char *current, bool next,
                   char *path, size_t pathSize);
bool formatFat32();
}
#endif
