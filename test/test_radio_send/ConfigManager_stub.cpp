// Minimal ConfigManager stub — returns safe defaults, no NVS needed
#include "../../include/ConfigManager.h"

void configInit() {}
void configReset() {}
void configSave() {}

const CanboxConfig* configGet() { return nullptr; }
const char* configGetVehicleFile() { return ""; }
void configSetVehicleFile(const char*) {}

int16_t  configGetSteerOffset()       { return 0; }
bool     configGetSteerInvert()       { return false; }
uint16_t configGetSteerScale()        { return 10000; }  // 1:1 ratio
uint16_t configGetIndicatorTimeout()  { return 500; }
uint8_t  configGetRpmDivisor()        { return 7; }
uint8_t  configGetTankCapacity()      { return 45; }
uint16_t configGetDteDivisor()        { return 338; }

void configSetSteerOffset(int16_t)    {}
void configSetSteerInvert(bool)       {}
void configSetSteerScale(uint16_t)    {}
void configSetIndicatorTimeout(uint16_t) {}
void configSetRpmDivisor(uint8_t)     {}
void configSetTankCapacity(uint8_t)   {}
void configSetDteDivisor(uint16_t)    {}
