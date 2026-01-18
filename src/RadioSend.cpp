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
    for (int i = 0; i < longueur; i++) { sum += donnees[i]; }
    uint8_t checksum = sum ^ 0xFF;

    RadioSerial.write(0x2E);      // Frame header (magic byte)
    RadioSerial.write(commande);  // Command ID
    RadioSerial.write(longueur);  // Payload length
    RadioSerial.write(donnees, longueur);  // Payload data
    RadioSerial.write(checksum);  // XOR checksum
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
    // Nissan Juke F15: 0.1 degree per unit (same as VW)
    // Sign is inverted (-) because Nissan and VW use opposite rotation conventions
    // If camera guidelines rotate the wrong way on screen, remove the minus sign
    int16_t angleVW = -currentSteer; 

    if (now - lastFastTime >= 100) {
        // Send as Little Endian (VW protocol standard)
        uint8_t payloadSteer[2] = { (uint8_t)(angleVW & 0xFF), (uint8_t)(angleVW >> 8) };
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
    
    // Remap door bits from internal format to VW Polo standard
    // Internal format (from CanCapture) -> VW format
    if (currentDoors & 0x80) vw |= 0x01;  // Driver (bit 7) -> VW Front Left (bit 0)
    if (currentDoors & 0x40) vw |= 0x02;  // Passenger (bit 6) -> VW Front Right (bit 1)
    if (currentDoors & 0x20) vw |= 0x04;  // Rear Left (bit 5) -> VW Rear Left (bit 2)
    if (currentDoors & 0x10) vw |= 0x08;  // Rear Right (bit 4) -> VW Rear Right (bit 3)
    if (currentDoors & 0x08) vw |= 0x10;  // Trunk (bit 3) -> VW Trunk (bit 4)

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
