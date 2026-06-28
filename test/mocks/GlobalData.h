#pragma once
#include <stdint.h>

// Minimal GlobalData stub for native test builds — only variables referenced
// by CanConfigProcessor::writeToGlobalData() are needed.

extern int16_t  currentSteer;
extern uint16_t engineRPM;
extern uint8_t  vehicleSpeed;
extern uint8_t  currentDoors;
extern uint8_t  fuelLevel;
extern float    voltBat;
extern int16_t  dteValue;
extern float    fuelConsoMoy;
extern int8_t   tempExt;
extern int8_t   coolantTemp;
extern uint32_t currentOdo;

extern bool indicatorLeft;
extern bool indicatorRight;
extern bool headlightsOn;
extern bool highBeamOn;
extern bool parkingLightsOn;
extern unsigned long lastLeftIndicatorTime;
extern unsigned long lastRightIndicatorTime;

extern uint16_t fuelConsumptionInst;
extern uint16_t fuelConsumptionAvg;
extern uint16_t averageSpeed;
extern uint16_t elapsedTime;

inline void resetVehicleData() {}
