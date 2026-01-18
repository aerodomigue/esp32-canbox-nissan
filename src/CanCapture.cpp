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
        // Format: Big Endian signed 16-bit value in bytes [1-2]
        // Center value: ~2912 (not zero when wheels straight)
        // Scaling and sign inversion handled in RadioSend
        case 0x002: 
            digitalWrite(LED_HEARTBEAT, !digitalRead(LED_HEARTBEAT)); 
            currentSteer = (int16_t)((rxFrame.data[1] << 8) | rxFrame.data[2]);
            break;

        // ======================================================================
        // 2. ENGINE RPM (CAN ID 0x180)
        // ======================================================================
        // Format: Big Endian unsigned 16-bit value
        // Bytes: [0-1] Raw RPM value
        // Scale factor: /7 (calibrated to match head unit display)
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
                // Byte [0]: Fuel level raw value
                // Inverted scale for Juke (255=empty, 0=full)
                // Mapped to 0-45L (Nissan Juke F15 tank capacity)
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
        // Byte [0] bit mapping (Nissan Juke/Qashqai):
        //   Bit 3 (0x08): Driver door
        //   Bit 4 (0x10): Passenger door
        //   Bit 5 (0x20): Rear left door
        //   Bit 6 (0x40): Rear right door
        // Byte [3] bit mapping:
        //   Bit 1 or 6 (0x42): Trunk/hatch
        // Remapped to generic bitmask for VW protocol compatibility
        case 0x60D: 
                // Save Handbrake state (Bit 0)
                uint8_t savedHandbrake = currentDoors & 0x01;
                currentDoors = savedHandbrake; 

                // NEW MAPPING (Based on Qashqai J10 Doc)
                // Byte 0, Bit 3 (0x08) -> Driver
                // Byte 0, Bit 4 (0x10) -> Passenger
                // Byte 0, Bit 5 (0x20) -> Rear Left
                // Byte 0, Bit 6 (0x40) -> Rear Right
                // Byte 3, Bit 1/5 (0x02/0x20) -> Trunk (Doc: D.2 or D.6)

                if (rxFrame.data[0] & 0x08) currentDoors |= 0x80; // Driver
                if (rxFrame.data[0] & 0x10) currentDoors |= 0x40; // Passenger
                if (rxFrame.data[0] & 0x20) currentDoors |= 0x20; // Rear Left
                if (rxFrame.data[0] & 0x40) currentDoors |= 0x10; // Rear Right
                
                // Test Trunk on Byte 3 (Mask 0x42 to catch bit 1 or 6)
                if (rxFrame.data[3] & 0x42) currentDoors |= 0x08; // Trunk
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