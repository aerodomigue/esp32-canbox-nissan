#ifndef CAN_AUX_DATA_H
#define CAN_AUX_DATA_H

#include <stdint.h>

// Additional CAN observations and availability, kept separate from radio units.
// Availability means a configured frame was received, not physical calibration.
struct CanAuxData {
    // F15 coolant arrives about every 100 ms. Stop repeating a lost source.
    static constexpr uint32_t COOLANT_TIMEOUT_MS = 2000;
    bool coolantValid = false;
    uint32_t coolantReceivedAt = 0;
    bool handbrake = false;
    bool handbrakeValid = false;
    bool brakePedal = false;
    bool brakePedalValid = false;
    uint8_t fuelCounterRaw = 0;
    bool fuelCounterValid = false;
    bool fuelLevelValid = false;
    bool fuelConsumptionInstValid = false;
    bool fuelConsumptionAvgValid = false;
    bool fuelConsumptionDerived = false;
    bool fuelConsumptionEstimated = false;
};

extern CanAuxData canAuxData;

#endif
