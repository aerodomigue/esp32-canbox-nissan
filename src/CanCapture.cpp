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
#include "ConfigManager.h"

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
        // Scale factor: configurable divisor (default 7)
        case 0x180:
            engineRPM = (uint16_t)((rxFrame.data[0] << 8) | rxFrame.data[1]) / configGetRpmDivisor();
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
                // Mapped to 0-tankCapacity (configurable, default 45L)
                uint8_t rawFuel = rxFrame.data[0];
                fuelLevel = map(rawFuel, 255, 0, 0, configGetTankCapacity());

                // Bytes [1-3]: Odometer reading in km (24-bit, for logging only)
                currentOdo = ((uint32_t)rxFrame.data[1] << 16) | 
                               ((uint32_t)rxFrame.data[2] << 8) | 
                               rxFrame.data[3];
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
        // 7. BODY CONTROL - Doors, Lights, Indicators (CAN ID 0x60D)
        // ======================================================================
        // Bytes [0-2] form a 24-bit status word (Nissan Juke/Qashqai B-platform)
        // Bit mapping:
        //   Bit 11: High beams
        //   Bit 13: Left indicator signal
        //   Bit 14: Right indicator signal
        //   Bit 17: Headlights (low beam)
        //   Bit 18: Parking lights
        //   Bit 19: Passenger door
        //   Bit 20: Driver door
        //   Bit 21: Rear left door
        //   Bit 22: Rear right door
        //   Bit 23: Boot/trunk
        case 0x60D:
            {
                // Build 24-bit status word from bytes 0-2
                uint32_t status = (uint32_t(rxFrame.data[0]) << 16) |
                                  (uint32_t(rxFrame.data[1]) << 8) |
                                  uint32_t(rxFrame.data[2]);

                // === LIGHTS ===
                highBeamOn = (status & (1UL << 11)) != 0;
                headlightsOn = (status & (1UL << 17)) != 0;
                parkingLightsOn = (status & (1UL << 18)) != 0;

                // === INDICATORS ===
                // Record timestamp when signal is active (for blink detection)
                if (status & (1UL << 13)) lastLeftIndicatorTime = now;
                if (status & (1UL << 14)) lastRightIndicatorTime = now;

                // === DOORS ===
                // Save Handbrake state (Bit 0) - preserved from other source
                uint8_t savedHandbrake = currentDoors & 0x01;
                currentDoors = savedHandbrake;

                if (status & (1UL << 20)) currentDoors |= 0x80; // Driver door (Front Left)
                if (status & (1UL << 19)) currentDoors |= 0x40; // Passenger door (Front Right)
                if (status & (1UL << 21)) currentDoors |= 0x20; // Rear Left
                if (status & (1UL << 22)) currentDoors |= 0x10; // Rear Right
                if (status & (1UL << 23)) currentDoors |= 0x08; // Boot/trunk
            }
            break;

        // ======================================================================
        // 8. TRIP COMPUTER - Distance to Empty (CAN ID 0x54C)
        // ======================================================================
        // Frame: 00 00 00 00 00 80 04 08
        // Bytes [6-7] big-endian: raw value, divide by configurable divisor
        // Example: 0x0408 = 1032 / 2.83 ≈ 365 km (divisor = 283)
        case 0x54C:
            {
                uint16_t rawDte = (rxFrame.data[6] << 8) | rxFrame.data[7];
                dteValue = (rawDte * 100) / configGetDteDivisor();
            }
            break;

        // ======================================================================
        // 9. FUEL CONSUMPTION (CAN ID 0x580)
        // ======================================================================
        // Frame: 00 00 81 00 3D 00 00 19
        // Byte [2]: Instantaneous consumption (0.1 L/100km, bit 7 may be flag)
        // Byte [4]: Average consumption (0.1 L/100km)
        // Note: At idle (speed=0), instant consumption is undefined
        case 0x580:
            // Mask off bit 7 in case it's a validity flag
            fuelConsumptionInst = rxFrame.data[2] & 0x7F;  // e.g., 0x81 → 1 (0.1 L/100km)
            fuelConsumptionAvg = rxFrame.data[4];          // e.g., 0x3D = 61 → 6.1 L/100km
            break;
    }
}