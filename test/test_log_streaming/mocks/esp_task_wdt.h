#pragma once
#include "Arduino.h"
struct esp_task_wdt_config_t {
    uint32_t timeout_ms;
    uint32_t idle_core_mask;
    bool trigger_panic;
};
inline void esp_task_wdt_deinit() {}
inline void esp_task_wdt_init(const esp_task_wdt_config_t*) {}
inline void esp_task_wdt_add(void*) {}
inline void esp_task_wdt_reset() { ++StreamingSimulation::watchdogResets; }
