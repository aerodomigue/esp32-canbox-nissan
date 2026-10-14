#ifndef CAN_AUX_DATA_H
#define CAN_AUX_DATA_H

#include <stdint.h>

// Additional CAN observations and availability, kept separate from radio units.
// Availability means a configured frame was received, not physical calibration.
struct CanAuxData {
    bool handbrake = false;
    bool handbrakeValid = false;
    bool brakePedal = false;
    bool brakePedalValid = false;
    uint8_t fuelCounterRaw = 0;
    bool fuelCounterValid = false;
    bool fuelLevelValid = false;
    bool fuelConsumptionInstValid = false;
    bool fuelConsumptionAvgValid = false;
};

extern CanAuxData canAuxData;

#endif
