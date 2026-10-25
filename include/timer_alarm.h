#pragma once

#include <driver/i2s.h>

bool startTimerAlarm(i2s_port_t port, uint8_t musicVolume);
void stopTimerAlarm(i2s_port_t port);
