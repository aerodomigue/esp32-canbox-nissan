/**
 * @file CanCapture.cpp
 * @brief CAN frame decoder for Nissan Juke F15
 * 
 * This module handles incoming CAN frames from the Nissan Juke F15 OBD-II bus
 * and decodes them into usable vehicle data. Each CAN ID contains specific
 * information encoded according to Nissan's proprietary format.
 * 
 * Data is stored in global variables (GlobalData.h) for use by RadioSend.
 * 
 * CAN Bus Specifications:
 * - Speed: 500 kbps
 * - Format: Standard 11-bit identifiers
 * - Byte order: Big Endian (MSB first) for most values
 */

#include <ESP32-TWAI-CAN.hpp>
#include "GlobalData.h"

#define LED_HEARTBEAT 8

/**
 * @brief Process a received CAN frame and extract vehicle data
 * @param rxFrame Reference to the received CAN frame
 */
void handleCanCapture(CanFrame &rxFrame) {
    // Timer for periodic debug logging (every 1 second)
    static uint32_t lastLogTime = 0;
    uint32_t now = millis();

    switch (rxFrame.identifier) {
        
        // ======================================================================
        // 1. STEERING WHEEL ANGLE (CAN ID 0x002)
        // ======================================================================
        // Format: Big Endian signed 16-bit value
        // Unit: 0.1 degrees per LSB
        // Range: -7200 to +7200 (±720 degrees, ~2 full turns)
        // Note: Sign inversion for VW protocol is handled in RadioSend
        case 0x002: 
            digitalWrite(LED_HEARTBEAT, !digitalRead(LED_HEARTBEAT)); 
            currentSteer = (int16_t)((rxFrame.data[1] << 8) | rxFrame.data[2]);
            break;

        // ======================================================================
        // 2. ENGINE RPM (CAN ID 0x180)
        // ======================================================================
        // Format: Big Endian unsigned 16-bit value
        // Bytes: [0-1] Raw RPM * 4
        // Scale factor: 0.25 (divide by 8/7 for head unit sync)
        case 0x180: 
            engineRPM = (uint16_t)((rxFrame.data[0] << 8) | rxFrame.data[1]) / 7;
            break;

        // ======================================================================
        // 3. VEHICLE SPEED - Wheel Speed (CAN ID 0x284)
        // ======================================================================
        // Format: Big Endian unsigned 16-bit value
        // Bytes: [0-1] Speed data (bytes [6-7] are counters, ignored)
        // Scale factor: 0.01 (divide by 100 to get km/h)
        case 0x284: 
            vehicleSpeed = (uint16_t)(((rxFrame.data[0] << 8) | rxFrame.data[1]) / 100);
            break;

        // ======================================================================
        // 4. INSTRUMENT CLUSTER DATA (CAN ID 0x5C5)
        // ======================================================================
        // Contains fuel level and odometer reading
        case 0x5C5: 
            {
                // Byte [0]: Fuel level as percentage (0-100%)
                // Fixed: Inverted scale for Juke (255=empty, 0=full) mapped to 45L
                uint8_t rawFuel = rxFrame.data[0];
                fuelLevel = map(rawFuel, 255, 0, 0, 45);

                // Bytes [1-3]: Odometer reading in km (24-bit, for logging only)
                uint32_t odo = ((uint32_t)rxFrame.data[1] << 16) | 
                               ((uint32_t)rxFrame.data[2] << 8) | 
                               rxFrame.data[3];
                (void)odo; // Suppress unused variable warning
            }
            break;

        // ======================================================================
        // 5. BATTERY VOLTAGE - Alternator Output (CAN ID 0x6F6)
        // ======================================================================
        // Format: Unsigned 8-bit value
        // Byte [0]: Voltage * 10 (e.g., 0x8D = 141 = 14.1V)
        // Scale factor: 0.1V per LSB
        case 0x6F6: 
            voltBat = rxFrame.data[0] * 0.1f;
            break;

        // ======================================================================
        // 6. COOLANT TEMPERATURE (CAN ID 0x551)
        // ======================================================================
        // Used as external temperature substitute (no dedicated exterior sensor)
        // Format: Unsigned 8-bit with -40°C offset
        // Byte [0]: Raw value, subtract 40 to get Celsius
        case 0x551:
            tempExt = (int8_t)(rxFrame.data[0] - 40);
            break;

        // ======================================================================
        // 7. BODY CONTROL - Door Status (CAN ID 0x60D)
        // ======================================================================
        // Byte [0] bit mapping (Nissan):
        //   Bit 0: Driver door
        //   Bit 1: Passenger door
        //   Bit 2: Rear left door
        //   Bit 3: Rear right door
        //   Bit 4: Trunk/hatch
        // Remapped to generic bitmask for VW protocol compatibility
        case 0x60D: 
            currentDoors = 0; 
            // Nissan Juke/370Z uses bits 4-7 for doors in Byte 0
            if (rxFrame.data[0] & 0x10) currentDoors |= 0x80; // Driver
            if (rxFrame.data[0] & 0x20) currentDoors |= 0x40; // Passenger
            if (rxFrame.data[0] & 0x40) currentDoors |= 0x20; // Rear Left
            if (rxFrame.data[0] & 0x80) currentDoors |= 0x10; // Rear Right
            // Trunk is often in data[0] bit 3 or data[1] bit 4
            if (rxFrame.data[1] & 0x10) currentDoors |= 0x08; 
            break;

        // ======================================================================
        // 8. TRIP COMPUTER - Distance to Empty (CAN ID 0x54C)
        // ======================================================================
        // Bytes [4-5]: Estimated remaining range in km
        case 0x54C: 
            dteValue = (rxFrame.data[4] << 8) | rxFrame.data[5]; 
            break;
    }

    // ==========================================================================
    // DEBUG LOGGING (Every 1 second when Serial is connected)
    // ==========================================================================
    if (Serial && (now - lastLogTime >= 1000)) {
        Serial.println("--- NISSAN DATA DECODED ---");
        Serial.printf("RPM: %u | Speed: %u | Volt: %.1fV | Temp: %d C\n", engineRPM, vehicleSpeed, voltBat, tempExt);
        Serial.printf("Fuel: %u L (VW scale) | Steer: %d\n", fuelLevel, currentSteer);
        Serial.printf("Doors Raw: 0x%02X\n", currentDoors);
        Serial.println("---------------------------");
        lastLogTime = now;
    }
}