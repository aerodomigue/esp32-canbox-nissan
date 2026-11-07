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
#define DEFAULT_STEER_SCALE         300     // Scale (300 = 0.0300x, unit x0.0001)
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
    uint16_t steerScale;        // Scale factor (1-20000, represents 0.0001x to 2.0x)
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

// Fields eligible for persistence in one configSave() call.
enum ConfigFields : uint8_t {
    CONFIG_STEER_OFFSET = 1 << 0,
    CONFIG_STEER_INVERT = 1 << 1,
    CONFIG_STEER_SCALE = 1 << 2,
    CONFIG_IND_TIMEOUT = 1 << 3,
    CONFIG_RPM_DIV = 1 << 4,
    CONFIG_TANK_CAP = 1 << 5,
    CONFIG_DTE_DIV = 1 << 6,
    CONFIG_ALL = 0x7F
};

/**
 * Save selected RAM values to NVS (all fields by default for CFG SAVE).
 * JSON reloads pass only their explicit keys. Values already equal in NVS
 * are not rewritten; unrelated temporary RAM adjustments remain unsaved.
 */
void configSave(uint8_t fields = CONFIG_ALL);

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
uint16_t configGetSteerScale();
uint16_t configGetIndicatorTimeout();
uint8_t  configGetRpmDivisor();
uint8_t  configGetTankCapacity();
uint16_t configGetDteDivisor();

// =============================================================================
// INDIVIDUAL SETTERS (RAM only; call configSave() to persist)
// =============================================================================

void configSetSteerOffset(int16_t value);
void configSetSteerInvert(bool value);
void configSetSteerScale(uint16_t value);
void configSetIndicatorTimeout(uint16_t value);
void configSetRpmDivisor(uint8_t value);
void configSetTankCapacity(uint8_t value);
void configSetDteDivisor(uint16_t value);

// =============================================================================
// VEHICLE CONFIG FILE (stored separately from calibration)
// =============================================================================

/**
 * @brief Get the last loaded vehicle config filename
 * @return Filename (e.g., "NissanJukeF15.json") or empty string if none saved
 */
const char* configGetVehicleFile();

/**
 * @brief Save the vehicle config filename to NVS
 * @param filename The filename to save (without leading /)
 */
void configSetVehicleFile(const char* filename);

#endif // CONFIG_MANAGER_H
