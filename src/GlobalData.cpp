#include "GlobalData.h"

// =============================================================================
// VEHICLE DATA STORAGE (memory allocation with safe defaults)
// =============================================================================

/** Steering wheel angle (0.1° units, signed) - 0 = centered */
int16_t currentSteer = 0;

/** Engine RPM - 0 = engine off */
uint16_t engineRPM = 0;

/** Vehicle speed in km/h - 0 = stationary */
uint8_t vehicleSpeed = 0;

/** Door status bitmask - 0 = all doors closed */
uint8_t currentDoors = 0;

/** Fuel level in liters (VW scale) - 0 = empty */
uint8_t fuelLevel = 0;

/** Battery voltage - 0.0V until first CAN reading */
float voltBat = 0.0;

/** Distance to empty in km - 0 = unknown */
int16_t dteValue = 0;

/** Average fuel consumption (reserved) - 0.0 = not calculated */
float fuelConsoMoy = 0.0;

/** Temperature in °C (from coolant sensor) - 0 = unknown */
int8_t tempExt = 0;

/** Odometer (Total Mileage) in km - 0 = unknown */
uint32_t currentOdo = 0;

// =============================================================================
// LIGHTS & INDICATORS
// =============================================================================

/** Left indicator - false = off */
bool indicatorLeft = false;

/** Right indicator - false = off */
bool indicatorRight = false;

/** Headlights (low beam) - false = off */
bool headlightsOn = false;

/** High beam - false = off */
bool highBeamOn = false;

/** Parking lights - false = off */
bool parkingLightsOn = false;

/** Last left indicator signal timestamp */
unsigned long lastLeftIndicatorTime = 0;

/** Last right indicator signal timestamp */
unsigned long lastRightIndicatorTime = 0;

// =============================================================================
// FUEL CONSUMPTION
// =============================================================================

/** Instantaneous fuel consumption (0.1 L/100km units) - 0 = unknown */
uint16_t fuelConsumptionInst = 0;

/** Average fuel consumption (0.1 L/100km units) - 0 = unknown */
uint16_t fuelConsumptionAvg = 0;

// =============================================================================
// TRIP COMPUTER DATA
// =============================================================================

/** Average speed in 0.1 km/h units - 0 = unknown */
uint16_t averageSpeed = 0;

/** Elapsed driving time in seconds - 0 = unknown */
uint16_t elapsedTime = 0;