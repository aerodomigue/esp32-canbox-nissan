#pragma once
#include "Arduino.h"
#define pdMS_TO_TICKS(ms) (ms)
inline void vTaskDelay(unsigned ms) { StreamingSimulation::advance(ms); }
