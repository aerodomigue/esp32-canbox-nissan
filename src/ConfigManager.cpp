/**
 * @file ConfigManager.cpp
 * @brief Implementation of persistent configuration using ESP32 NVS
 */

#include "ConfigManager.h"
#include <Preferences.h>

// =============================================================================
// PRIVATE VARIABLES
// =============================================================================

static Preferences prefs;
static CanboxConfig config;
static const char* NVS_NAMESPACE = "canbox";

// NVS keys (max 15 chars)
static const char* KEY_STEER_OFFSET    = "steerOffset";
static const char* KEY_STEER_INVERT    = "steerInvert";
static const char* KEY_STEER_SCALE     = "steerScale";
static const char* KEY_IND_TIMEOUT     = "indTimeout";
static const char* KEY_RPM_DIVISOR     = "rpmDiv";
static const char* KEY_TANK_CAPACITY   = "tankCap";
static const char* KEY_DTE_DIVISOR     = "dteDiv";

// =============================================================================
// PRIVATE FUNCTIONS
// =============================================================================

static void loadDefaults() {
    config.steerOffset      = DEFAULT_STEER_OFFSET;
    config.steerInvert      = DEFAULT_STEER_INVERT;
    config.steerScale       = DEFAULT_STEER_SCALE;
    config.indicatorTimeout = DEFAULT_INDICATOR_TIMEOUT;
    config.rpmDivisor       = DEFAULT_RPM_DIVISOR;
    config.tankCapacity     = DEFAULT_TANK_CAPACITY;
    config.dteDivisor       = DEFAULT_DTE_DIVISOR;
}

// =============================================================================
// PUBLIC API IMPLEMENTATION
// =============================================================================

void configInit() {
    // Load defaults first
    loadDefaults();

    // Open NVS namespace (read-only first to check if values exist)
    if (prefs.begin(NVS_NAMESPACE, true)) {
        // Load saved values (use current defaults if key doesn't exist)
        config.steerOffset      = prefs.getShort(KEY_STEER_OFFSET, config.steerOffset);
        config.steerInvert      = prefs.getBool(KEY_STEER_INVERT, config.steerInvert);
        config.steerScale       = prefs.getUChar(KEY_STEER_SCALE, config.steerScale);
        config.indicatorTimeout = prefs.getUShort(KEY_IND_TIMEOUT, config.indicatorTimeout);
        config.rpmDivisor       = prefs.getUChar(KEY_RPM_DIVISOR, config.rpmDivisor);
        config.tankCapacity     = prefs.getUChar(KEY_TANK_CAPACITY, config.tankCapacity);
        config.dteDivisor       = prefs.getUShort(KEY_DTE_DIVISOR, config.dteDivisor);
        prefs.end();
    }
    // If prefs.begin() fails, we just use defaults (already loaded)
}

void configSave() {
    if (prefs.begin(NVS_NAMESPACE, false)) { // false = read-write mode
        prefs.putShort(KEY_STEER_OFFSET, config.steerOffset);
        prefs.putBool(KEY_STEER_INVERT, config.steerInvert);
        prefs.putUChar(KEY_STEER_SCALE, config.steerScale);
        prefs.putUShort(KEY_IND_TIMEOUT, config.indicatorTimeout);
        prefs.putUChar(KEY_RPM_DIVISOR, config.rpmDivisor);
        prefs.putUChar(KEY_TANK_CAPACITY, config.tankCapacity);
        prefs.putUShort(KEY_DTE_DIVISOR, config.dteDivisor);
        prefs.end();
    }
}

void configReset() {
    loadDefaults();

    // Clear NVS namespace and save defaults
    if (prefs.begin(NVS_NAMESPACE, false)) {
        prefs.clear();
        prefs.end();
    }
    configSave();
}

const CanboxConfig* configGet() {
    return &config;
}

// =============================================================================
// GETTERS
// =============================================================================

int16_t configGetSteerOffset() {
    return config.steerOffset;
}

bool configGetSteerInvert() {
    return config.steerInvert;
}

uint8_t configGetSteerScale() {
    return config.steerScale;
}

uint16_t configGetIndicatorTimeout() {
    return config.indicatorTimeout;
}

uint8_t configGetRpmDivisor() {
    return config.rpmDivisor;
}

uint8_t configGetTankCapacity() {
    return config.tankCapacity;
}

uint16_t configGetDteDivisor() {
    return config.dteDivisor;
}

// =============================================================================
// SETTERS
// =============================================================================

void configSetSteerOffset(int16_t value) {
    config.steerOffset = value;
}

void configSetSteerInvert(bool value) {
    config.steerInvert = value;
}

void configSetSteerScale(uint8_t value) {
    config.steerScale = value;
}

void configSetIndicatorTimeout(uint16_t value) {
    config.indicatorTimeout = value;
}

void configSetRpmDivisor(uint8_t value) {
    config.rpmDivisor = value;
}

void configSetTankCapacity(uint8_t value) {
    config.tankCapacity = value;
}

void configSetDteDivisor(uint16_t value) {
    config.dteDivisor = value;
}
