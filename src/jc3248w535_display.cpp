#if defined(BOARD_JC3248W535)
#include "jc3248w535_display.h"

Arduino_ESP32QSPI jc3248w535::displayBus(45, 47, 21, 48, 40, 39);
Arduino_AXS15231B jc3248w535::displayPanel(
    &jc3248w535::displayBus, GFX_NOT_DEFINED, 0, false, 320, 480);
#endif
