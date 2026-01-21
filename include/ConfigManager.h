/**
 * @file ConfigManager.h
 * @brief Persistent configuration storage using ESP32 NVS (Preferences)
 *
 * Stores vehicle-specific calibration values in non-volatile storage.
 * Values persist across reboots. Use configReset() to restore defaults.
 */

#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>

// =============================================================================
// DEFAULT VALUES (Nissan Juke F15)
// =============================================================================

#define DEFAULT_STEER_OFFSET        100     // Center offset for steering
#define DEFAULT_STEER_INVERT        true    // Invert steering direction
#define DEFAULT_STEER_SCALE         4       // Scale percentage (4 = 0.04x)
#define DEFAULT_INDICATOR_TIMEOUT   500     // Indicator timeout in ms
#define DEFAULT_RPM_DIVISOR         7       // RPM = raw_value / 7
#define DEFAULT_TANK_CAPACITY       45      // Tank size in liters
#define DEFAULT_DTE_DIVISOR         283     // DTE = raw * 100 / 283

// =============================================================================
// CONFIGURATION STRUCTURE
// =============================================================================

struct CanboxConfig {
    int16_t  steerOffset;       // Steering center offset (-500 to +500)
    bool     steerInvert;       // Invert steering direction
    uint8_t  steerScale;        // Scale factor (1-200, represents 0.01x to 2.0x)
    uint16_t indicatorTimeout;  // Indicator off timeout in ms
    uint8_t  rpmDivisor;        // RPM divisor (typically 7)
    uint8_t  tankCapacity;      // Fuel tank capacity in liters
    uint16_t dteDivisor;        // DTE divisor * 100 (283 = 2.83)
};

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize and load configuration from NVS
 * Call once in setup(). Loads saved values or defaults if first boot.
 */
void configInit();

/**
 * @brief Save all current config values to NVS
 * Call after modifying config values to persist them.
 */
void configSave();

/**
 * @brief Reset all config values to defaults and save to NVS
 */
void configReset();

/**
 * @brief Get pointer to current config (read-only access)
 */
const CanboxConfig* configGet();

// =============================================================================
// INDIVIDUAL GETTERS (for convenience)
// =============================================================================

int16_t  configGetSteerOffset();
bool     configGetSteerInvert();
uint8_t  configGetSteerScale();
uint16_t configGetIndicatorTimeout();
uint8_t  configGetRpmDivisor();
uint8_t  configGetTankCapacity();
uint16_t configGetDteDivisor();

// =============================================================================
// INDIVIDUAL SETTERS (automatically marks config as dirty, call configSave() to persist)
// =============================================================================

void configSetSteerOffset(int16_t value);
void configSetSteerInvert(bool value);
void configSetSteerScale(uint8_t value);
void configSetIndicatorTimeout(uint16_t value);
void configSetRpmDivisor(uint8_t value);
void configSetTankCapacity(uint8_t value);
void configSetDteDivisor(uint16_t value);

#endif // CONFIG_MANAGER_H
