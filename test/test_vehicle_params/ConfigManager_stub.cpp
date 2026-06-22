/**
 * ConfigManager stub — provides function definitions for native tests.
 * Delegates all calls to g_mock for assertion in test cases.
 *
 * The real include/ConfigManager.h provides declarations;
 * this file provides the definitions (no header shadowing needed).
 */
#include "../../include/ConfigManager.h"
#include "../mocks/ConfigManager_mock.h"

MockConfigState g_mock;

void configInit() {}

void configReset() {
    g_mock.steerScale       = 300;
    g_mock.steerOffset      = 100;
    g_mock.steerInvert      = true;
    g_mock.indicatorTimeout = 500;
    g_mock.rpmDivisor       = 7;
    g_mock.tankCapacity     = 45;
    g_mock.dteDivisor       = 283;
    g_mock.steerScaleSet       = false;
    g_mock.steerOffsetSet      = false;
    g_mock.steerInvertSet      = false;
    g_mock.indicatorTimeoutSet = false;
    g_mock.rpmDivisorSet       = false;
    g_mock.tankCapacitySet     = false;
    g_mock.dteDivisorSet       = false;
    g_mock.resetCount++;
}

void configSave() {
    g_mock.saveCount++;
}

const CanboxConfig* configGet() { return nullptr; }

const char* configGetVehicleFile() { return g_mock.vehicleFile; }
void configSetVehicleFile(const char* f) {
    strncpy(g_mock.vehicleFile, f, sizeof(g_mock.vehicleFile) - 1);
    g_mock.vehicleFile[sizeof(g_mock.vehicleFile) - 1] = '\0';
}

int16_t  configGetSteerOffset()      { return g_mock.steerOffset; }
bool     configGetSteerInvert()      { return g_mock.steerInvert; }
uint16_t configGetSteerScale()       { return g_mock.steerScale; }
uint16_t configGetIndicatorTimeout() { return g_mock.indicatorTimeout; }
uint8_t  configGetRpmDivisor()       { return g_mock.rpmDivisor; }
uint8_t  configGetTankCapacity()     { return g_mock.tankCapacity; }
uint16_t configGetDteDivisor()       { return g_mock.dteDivisor; }

void configSetSteerOffset(int16_t v)      { g_mock.steerOffset = v; g_mock.steerOffsetSet = true; }
void configSetSteerInvert(bool v)         { g_mock.steerInvert = v; g_mock.steerInvertSet = true; }
void configSetSteerScale(uint16_t v)      { g_mock.steerScale = v; g_mock.steerScaleSet = true; }
void configSetIndicatorTimeout(uint16_t v){ g_mock.indicatorTimeout = v; g_mock.indicatorTimeoutSet = true; }
void configSetRpmDivisor(uint8_t v)       { g_mock.rpmDivisor = v; g_mock.rpmDivisorSet = true; }
void configSetTankCapacity(uint8_t v)     { g_mock.tankCapacity = v; g_mock.tankCapacitySet = true; }
void configSetDteDivisor(uint16_t v)      { g_mock.dteDivisor = v; g_mock.dteDivisorSet = true; }
