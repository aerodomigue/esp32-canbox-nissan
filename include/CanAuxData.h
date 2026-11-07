#ifndef CAN_AUX_DATA_H
#define CAN_AUX_DATA_H

#include <stdint.h>

// Additional CAN observations and availability, kept separate from radio units.
// Availability means a configured frame was received, not physical calibration.
struct CanAuxData {
    // F15 coolant arrives about every 100 ms. Stop repeating a lost source.
    static constexpr uint32_t COOLANT_TIMEOUT_MS = 2000;
    static constexpr uint32_t SOURCE_TIMEOUT_MS = 2000;
    static constexpr uint32_t OIL_TIMEOUT_MS = 2000;
    static constexpr uint32_t DTE_TIMEOUT_MS = 2000;
    bool duduReceiver = false;
    bool rpmValid = false, speedValid = false, odometerValid = false;
    bool outsideValid = false, dteValid = false, oilValid = false;
    uint32_t rpmReceivedAt = 0, speedReceivedAt = 0, odometerReceivedAt = 0;
    uint32_t outsideReceivedAt = 0, dteReceivedAt = 0, oilReceivedAt = 0;
    uint16_t speedCentiKmh = 0;
    int16_t oilTemperatureC = 0;
    bool oilEstimated = false;
    bool bootTripValid = false;
    bool fuelBootTripComplete = true;
    uint32_t bootTripDistanceDeciKm = 0;
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
