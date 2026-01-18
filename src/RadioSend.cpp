/**
 * @file RadioSend.cpp
 * @brief VW-protocol radio communication for Android head unit
 * 
 * This module translates decoded Nissan CAN data into the VW Polo protocol
 * format expected by aftermarket Android head units. Communication occurs
 * over UART at 38400 baud.
 * 
 * VW Protocol Frame Format:
 * ┌──────┬─────────┬────────┬──────────────┬──────────┐
 * │ 0x2E │ Command │ Length │ Payload[n]   │ Checksum │
 * │ HEAD │  (1B)   │  (1B)  │ (Length B)   │   (1B)   │
 * └──────┴─────────┴────────┴──────────────┴──────────┘
 * 
 * Checksum calculation: (Command + Length + Sum(Payload)) XOR 0xFF
 * 
 * Key Commands:
 * - 0x26: Steering wheel angle
 * - 0x41: Dashboard data (sub-commands 0x01=doors, 0x02=gauges)
 * - 0xF1: Version response (handshake)
 * - 0x91: Acknowledgment response
 */

#include <Arduino.h>
#include "GlobalData.h"

extern HardwareSerial RadioSerial;

// Timer for debug logging
static uint32_t lastLogRadio = 0;

/**
 * @brief Transmit a data frame to the head unit
 * @param command Command byte (determines data type)
 * @param data Pointer to payload data array
 * @param length Number of bytes in payload
 * 
 * Builds and sends a complete VW protocol frame with header, command,
 * length, payload, and checksum.
 */
void transmettreVersPoste(uint8_t commande, uint8_t* donnees, uint8_t longueur) {
    uint8_t sum = commande + longueur;
    for (int i = 0; i < longueur; i++) { 
        sum += donnees[i]; 
    }
    
    // Exact checksum calculation according to Junsun/Raise PSA doc:
    // 0xFF - sum of (command + length + data)
    uint8_t checksum = (uint8_t)(0xFF - sum);

    RadioSerial.write(0x2E);      // Header
    RadioSerial.write(commande);  // Function ID
    RadioSerial.write(longueur);  // Payload length
    RadioSerial.write(donnees, longueur);  // Payload
    RadioSerial.write(checksum);  // Corrected Checksum
}

/**
 * @brief Handle protocol handshake with the head unit
 * 
 * Responds to initialization requests from the Android head unit:
 * - 0xC0/0x08: Version query -> Reply with firmware version (0xF1)
 * - 0x90: Status query -> Reply with acknowledgment (0x91)
 * 
 * Must be called regularly to maintain communication link.
 */
void handshake() {
    while (RadioSerial.available() > 0) {
        uint8_t head = RadioSerial.read();
        if (head == 0x2E) {  // Valid frame header detected
            delay(5);  // Wait for complete frame reception
            uint8_t cmd = RadioSerial.read();
            uint8_t len = RadioSerial.read();
            // Consume remaining payload + checksum
            for (int i = 0; i < len + 1; i++) { RadioSerial.read(); }
            
            // Respond to version/init requests (0xC0 or 0x08)
            if (cmd == 0xC0 || cmd == 0x08) {
                uint8_t ver[] = {0x02, 0x08, 0x10};  // Firmware version identifier
                transmettreVersPoste(0xF1, ver, 3);  // Send version response
            }
            // Respond to status query (0x90)
            if (cmd == 0x90) {
                uint8_t ok[] = {0x41, 0x02};  // Status OK
                transmettreVersPoste(0x91, ok, 2);  // Send acknowledgment
            }
        }
    }
}

/**
 * @brief Main update function - sends all vehicle data to the radio
 * 
 * Update intervals:
 * - Steering angle: 100ms (fast update for smooth camera guidelines)
 * - Dashboard gauges: 400ms (RPM, speed, battery, temperature, fuel)
 * - Door status: On change only (reduces bus traffic)
 */
void processRadioUpdates() {
    static unsigned long lastFastTime = 0;
    static unsigned long lastSlowTime = 0;
    unsigned long now = millis();

    handshake();

    // ==========================================================================
    // 1. STEERING WHEEL ANGLE (Command 0x26) - 100ms interval
    // ==========================================================================
    /**
     * DYNAMIC GUIDELINES CALIBRATION (IPAS):
     * 1. NISSAN OFFSET: The Juke F15 sensor is not at 0 when wheels are straight.
     * Logs show ~2912 (0x0B60). We center it by adding a +100 fine-tuning adjustment.
     * * 2. SCALING: 
     * For the dynamic lines to behave correctly in reverse on this Android head unit, 
     * we must NOT send the actual steering angle (540°).
     * TARGET: +/- 200 degrees (Value 2000) at mechanical full lock.
     * * Calculation: Nissan Max ~5400 * 0.04 = 216 (represents 21.6° displayed, 216° interpreted).
     * The 0.04 factor is validated to achieve this +/- 200° range.
     * * 3. DIRECTION: Sign inversion (-) required to match VW orientation.
     */

    // Step 1: Remove Nissan center offset (2912 found in logs)
    int32_t centeredSteer = (int32_t)currentSteer + 100;

    // Step 2: Scale to VW protocol (-5400 to +5400 range)
    // Multiplier 2.7 ensures the lines reach the screen edges at full lock
    int16_t angleVW = (int16_t)(centeredSteer * 0.04f);

    // Step 3: Invert sign to match VW direction (Right = Positive)
    // Switch to 'angleVW = angleVW;' if lines move backwards
    angleVW = -angleVW; 

    if (now - lastFastTime >= 100) {
        // VW Protocol 0x26 expects 2 bytes in Little Endian format
        uint8_t payloadSteer[2];
        payloadSteer[0] = (uint8_t)(angleVW & 0xFF);         // LSB
        payloadSteer[1] = (uint8_t)((angleVW >> 8) & 0xFF);  // MSB
        
        transmettreVersPoste(0x26, payloadSteer, 2); 
        lastFastTime = now;
    }   

    // ==========================================================================
    // 2. DASHBOARD DATA (Command 0x41, Sub-command 0x02) - 400ms interval
    // ==========================================================================
    // Contains: Speed, RPM, Battery voltage, Temperature, Fuel level, Status flags
    if (now - lastSlowTime >= 400) {
        int16_t tRaw = (int16_t)(tempExt * 10.0f);        // Temperature * 10 for decimal precision
        uint16_t vBatScaled = (uint16_t)(voltBat * 100.0f); // Voltage * 100 for decimal precision
        
        // Speed conversion: km/h -> VW scale (*100)
        uint16_t speedScaled = vehicleSpeed * 100; 

        uint8_t payload41[13] = {0};
        payload41[0]  = 0x02;  // Sub-command: Dashboard gauges
        
        // RPM (Bytes 1-2) - Big Endian
        payload41[1]  = (uint8_t)(engineRPM >> 8);     // MSB
        payload41[2]  = (uint8_t)(engineRPM & 0xFF);   // LSB

        // SPEED (Bytes 3-4) - Little Endian
        payload41[3]  = (uint8_t)(speedScaled & 0xFF); // LSB
        payload41[4]  = (uint8_t)(speedScaled >> 8);   // MSB

        // BATTERY VOLTAGE (Bytes 5-6) - Big Endian
        payload41[5]  = (uint8_t)(vBatScaled >> 8);    // MSB
        payload41[6]  = (uint8_t)(vBatScaled & 0xFF);  // LSB

        // TEMPERATURE (Bytes 7-8) - Big Endian, signed
        payload41[7]  = (uint8_t)(tRaw >> 8);          // MSB
        payload41[8]  = (uint8_t)(tRaw & 0xFF);        // LSB

        // STATUS FLAGS (Byte 11)
        // Bit 0: Handbrake engaged
        // Bit 2: Engine running / Ignition on
        uint8_t statusByte = 0x04;  // Default: Engine running flag set

        // HANDBRAKE STATUS (Bit 0 of byte 11)
        // Retrieved from currentDoors bit 0 (see CanCapture mapping)
        if (currentDoors & 0x01) {
             statusByte |= 0x01;  // Set handbrake bit
        }
        payload41[11] = statusByte; 

        // FUEL LEVEL (Byte 12) - Already scaled to VW tank size in CanCapture
        payload41[12] = (uint8_t)fuelLevel;

        transmettreVersPoste(0x41, payload41, 13);
        lastSlowTime = now;
    }

    // ==========================================================================
    // 3. DOOR STATUS (Command 0x41, Sub-command 0x01) - On change only
    // ==========================================================================
    static uint8_t lastSentDoors = 0xFF;  // Init to invalid value to force first send
    uint8_t vw = 0;
    
    // Internal format (currentDoors) -> VW format (vw)
    if (currentDoors & 0x80) vw |= 0x01;  // VW Front Left (Driver)
    if (currentDoors & 0x40) vw |= 0x02;  // VW Front Right (Passenger)
    if (currentDoors & 0x20) vw |= 0x04;  // VW Rear Left
    if (currentDoors & 0x10) vw |= 0x08;  // VW Rear Right
    if (currentDoors & 0x08) vw |= 0x10;  // VW Trunk

    // Only send when door status changes (reduces unnecessary traffic)
    if (currentDoors != lastSentDoors) {
        uint8_t payload[13] = {0};
        payload[0] = 0x01;  // Sub-command: Door status
        payload[1] = vw;    // Door bitmask (handbrake bit 0x01 ignored in this context)
        transmettreVersPoste(0x41, payload, 13);
        lastSentDoors = currentDoors;
    }

    // ==========================================================================
    // 4. DEBUG LOGGING (Every 1 second when Serial is connected)
    // ==========================================================================
    if (Serial && (now - lastLogRadio >= 1000)) {
        Serial.println(">>> RADIO TX STATUS >>>");
        Serial.printf("RPM: %u | Fuel: %u%%\n", engineRPM, fuelLevel);
        Serial.printf("VW Steer: %d | VW Doors: 0x%02X\n", angleVW, vw);
        Serial.printf("VW Bat Raw: %u | VW Temp Raw: %d\n", (uint16_t)(voltBat * 100), (int16_t)(tempExt * 10));
        Serial.println("-----------------------");
        lastLogRadio = now;
    }
}
