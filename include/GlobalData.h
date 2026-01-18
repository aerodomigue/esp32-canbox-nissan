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

#endif
