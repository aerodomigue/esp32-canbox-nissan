/**
 * @file GlobalData.h
 * @brief Shared vehicle data declarations
 * 
 * This header declares all global variables used to store vehicle data
 * decoded from the CAN bus. Variables are defined in GlobalData.cpp
 * and accessed by both CanCapture (write) and RadioSend (read).
 * 
 * All variables use 'extern' to allow multiple translation units to
 * share the same data without linker errors.
 */

#ifndef GLOBAL_DATA_H
#define GLOBAL_DATA_H

#include <Arduino.h>

// =============================================================================
// SHARED VEHICLE DATA (extern declarations)
// =============================================================================

/** Steering wheel angle in 0.1 degree units (signed, ±720°) */
extern int16_t currentSteer;

/** Engine speed in RPM (0-8000 typical range) */
extern uint16_t engineRPM;

/** Vehicle speed in km/h (0-255) */
extern uint8_t vehicleSpeed;

/** Door status bitmask (see CanCapture.cpp for bit mapping) */
extern uint8_t currentDoors;

/** Fuel level in liters, scaled to VW Polo tank (0-45L) */
extern uint8_t fuelLevel;

/** Battery/alternator voltage in volts (typically 12.0-14.5V) */
extern float voltBat;

/** Distance to empty in km (estimated remaining range) */
extern int16_t dteValue;

/** Average fuel consumption (L/100km) - reserved for future use */
extern float fuelConsoMoy;

/** External temperature in °C (actually coolant temp, used as substitute) */
extern int8_t tempExt;

/** Odometer (Total Mileage) in km - 0 = unknown */
extern uint32_t currentOdo;

// =============================================================================
// LIGHTS & INDICATORS (from CAN 0x60D)
// =============================================================================

/** Left indicator active (blinker) */
extern bool indicatorLeft;

/** Right indicator active (blinker) */
extern bool indicatorRight;

/** Headlights on (low beam) */
extern bool headlightsOn;

/** High beam on (full beam) */
extern bool highBeamOn;

/** Parking lights on (position lights) */
extern bool parkingLightsOn;

/** Timestamp of last left indicator signal (for timeout detection) */
extern unsigned long lastLeftIndicatorTime;

/** Timestamp of last right indicator signal (for timeout detection) */
extern unsigned long lastRightIndicatorTime;

// =============================================================================
// FUEL CONSUMPTION (from CAN 0x580)
// =============================================================================

/** Instantaneous fuel consumption in 0.1 L/100km units (e.g., 75 = 7.5 L/100km) */
extern uint16_t fuelConsumptionInst;

/** Average fuel consumption in 0.1 L/100km units */
extern uint16_t fuelConsumptionAvg;

// =============================================================================
// TRIP COMPUTER DATA
// =============================================================================

/** Average speed in 0.1 km/h units (e.g., 450 = 45.0 km/h) - for trip display */
extern uint16_t averageSpeed;

/** Elapsed driving time in seconds - for trip display */
extern uint16_t elapsedTime;

// =============================================================================
// UTILITY FUNCTIONS
// =============================================================================

/**
 * @brief Reset all vehicle data to default values
 *
 * Call this when loading a new CAN configuration to clear stale data
 * from the previous configuration.
 */
void resetVehicleData();

#endif
