#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include <Arduino.h>

// Variables partagées (déclarées en extern pour être accessibles partout)
extern int16_t currentSteer;
extern uint16_t engineRPM;
extern uint8_t vehicleSpeed;
extern uint8_t currentDoors;
extern uint8_t fuelLevel;
extern float voltBat;
extern int16_t dteValue;
extern float fuelConsoMoy;
extern int8_t tempExt;

#endif