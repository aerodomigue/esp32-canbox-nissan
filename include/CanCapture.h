/**
 * @file CanCapture.h
 * @brief CAN frame decoder for Nissan Juke F15
 * 
 * Declares the handler function for processing incoming CAN frames
 * and extracting vehicle data (RPM, speed, steering, doors, etc.)
 */

#ifndef CAN_CAPTURE_H
#define CAN_CAPTURE_H

#include <ESP32-TWAI-CAN.hpp>

/**
 * @brief Process a received CAN frame and extract vehicle data
 * @param rxFrame Reference to the received CAN frame
 * 
 * Supported CAN IDs:
 * - 0x002: Steering wheel angle
 * - 0x180: Engine RPM
 * - 0x284: Vehicle speed (active preset bytes 0-1, raw / 100)
 * - 0x5C5: Fuel level and odometer
 * - 0x6F6: Battery voltage (alternator)
 * - 0x540: External temperature
 * - 0x551: Coolant temperature
 * - 0x60D: Door status
 * - 0x54C: Distance to empty (DTE, active preset)
 */
void handleCanCapture(const CanFrame& rxFrame, uint32_t receivedAt);

#endif
