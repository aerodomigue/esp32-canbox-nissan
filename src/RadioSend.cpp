/**
 * @file RadioSend.cpp
 * @brief VW Polo / Raise protocol communication for Android head unit
 * 
 * This module translates decoded Nissan CAN data into the VW Polo protocol
 * format expected by aftermarket Android head units. Communication occurs
 * over UART at 38400 baud, 8N1 format.
 * 
 * Protocol specification: Raise VW Polo Protocol v1.4
 * Documentation: docs/protocols/raise-vw-polo/
 * 
 * VW Protocol Frame Format:
 * ┌──────┬─────────┬────────┬──────────────┬──────────┐
 * │ 0x2E │ DataType│ Length │ Payload[n]   │ Checksum │
 * │ HEAD │  (1B)   │  (1B)  │ (Length B)   │   (1B)   │
 * └──────┴─────────┴────────┴──────────────┴──────────┘
 * 
 * Checksum calculation (official spec):
 * Checksum = (DataType + Length + Data0 + ... + DataN) XOR 0xFF
 * 
 * Key DataTypes (SLAVE → HOST):
 * - 0x26: Steering wheel angle
 * - 0x41: Body information (sub-commands: 0x01=doors/safety, 0x02=gauges)
 * 
 * Handshake (HOST → SLAVE):
 * - 0x81: Start/End connection (respond with ACK 0xFF)
 * - 0x90: Request control information (respond with requested data)
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
    // Calculate checksum according to official Raise VW Polo protocol v1.4:
    // Checksum = (DataType + Length + Data0 + ... + DataN) XOR 0xFF
    uint8_t sum = commande + longueur;
    for (int i = 0; i < longueur; i++) { 
        sum += donnees[i]; 
    }
    uint8_t checksum = sum ^ 0xFF;

    RadioSerial.write(0x2E);      // Header (fixed 0x2E)
    RadioSerial.write(commande);  // DataType
    RadioSerial.write(longueur);  // Length
    RadioSerial.write(donnees, longueur);  // Payload
    RadioSerial.write(checksum);   // Checksum
}

/**
 * @brief Handle protocol handshake with the head unit
 * 
 * Responds to commands from the Android head unit (HOST) according to
 * official Raise VW Polo protocol v1.4:
 * 
 * - 0x81: Start/End connection
 *   - Data0 = 0x01: Start -> Respond with ACK (0xFF)
 *   - Data0 = 0x00: End -> Respond with ACK (0xFF)
 * 
 * - 0x90: Request control information
 *   - Data0 specifies which data type to send (0x14, 0x16, 0x21, 0x24, 0x25, 0x26, 0x30, 0x41)
 *   - Respond by sending the requested data type
 * 
 * Must be called regularly to maintain communication link.
 */
void handshake() {
    while (RadioSerial.available() > 0) {
        uint8_t head = RadioSerial.read();
        if (head == 0x2E) {  // Valid frame header detected
            delay(5);  // Wait for complete frame reception
            uint8_t dataType = RadioSerial.read();
            uint8_t len = RadioSerial.read();
            
            // Read payload
            uint8_t payload[16] = {0};
            for (int i = 0; i < len && i < 16; i++) {
                payload[i] = RadioSerial.read();
            }
            // Consume checksum
            RadioSerial.read();
            
            // Handle Start/End (0x81)
            if (dataType == 0x81 && len >= 1) {
                // Data0: 0x01 = Start, 0x00 = End
                // Respond with ACK (0xFF) as per protocol
                RadioSerial.write(0xFF);
            }
            
            // Handle Request Control Information (0x90)
            if (dataType == 0x90 && len >= 1) {
                uint8_t requestedType = payload[0];
                // Data0 specifies which data type to send
                // For now, we send data proactively, so we can ignore most requests
                // But we could implement specific responses here if needed
                // Example: if (requestedType == 0x26) { send steering angle }
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

    // ======================================================================
    // 1. STEERING WHEEL ANGLE (DataType 0x26) - 100ms interval
    // ======================================================================
    // Format: Signed 16-bit value, Little Endian
    // Bytes: [0] = ESP1 (low byte), [1] = ESP2 (high byte)
    // Protocol: ESP = 0 → centered, ESP > 0 → left, ESP < 0 → right
    // 
    // DYNAMIC GUIDELINES CALIBRATION (IPAS):
    // 1. NISSAN OFFSET: Juke F15 sensor is not at 0 when wheels straight
    //    Center value: ~2912 (0x0B60) from logs, fine-tuned with +100 adjustment
    // 2. SCALING: Must NOT send actual steering angle (540° full lock)
    //    Target: +/- 200 degrees (value 2000) at mechanical full lock
    //    Calculation: Nissan Max ~5400 * 0.04 = 216 (21.6° displayed, 216° interpreted)
    //    Scale factor 0.04 validated to achieve +/- 200° range
    // 3. DIRECTION: Sign inversion required to match VW orientation (Right = Positive)
    if (now - lastFastTime >= 100) {
        // Step 1: Remove Nissan center offset (2912 found in logs)
        int32_t centeredSteer = (int32_t)currentSteer + 100;

        // Step 2: Scale to VW protocol range (-5400 to +5400)
        int16_t angleVW = (int16_t)(centeredSteer * 0.04f);

        // Step 3: Invert sign to match VW direction
        angleVW = -angleVW; 

        // Format payload: Data0 = ESP1 (LSB), Data1 = ESP2 (MSB)
        uint8_t payloadSteer[2];
        payloadSteer[0] = (uint8_t)(angleVW & 0xFF);         // Data0: ESP1 (low byte)
        payloadSteer[1] = (uint8_t)((angleVW >> 8) & 0xFF);  // Data1: ESP2 (high byte)
        
        transmettreVersPoste(0x26, payloadSteer, 2); 
        lastFastTime = now;
    }   

    // ======================================================================
    // 2. DASHBOARD DATA (DataType 0x41, Command 0x02) - 400ms interval
    // ======================================================================
    // Format: 13 bytes total, Big Endian for multi-byte values
    // Protocol specification: Raise VW Polo Protocol v1.4
    if (now - lastSlowTime >= 400) {
        uint8_t payload41[13] = {0};
        payload41[0] = 0x02;  // Command: Dashboard gauges
        
        // Bytes [1-2]: Engine RPM
        // Format: Big Endian unsigned 16-bit
        // Formula: RPM = Data1 * 256 + Data2
        payload41[1] = (uint8_t)(engineRPM >> 8);   // Data1: MSB
        payload41[2] = (uint8_t)(engineRPM & 0xFF);  // Data2: LSB
        
        // Bytes [3-4]: Vehicle speed
        // Format: Big Endian unsigned 16-bit
        // Formula: Speed (km/h) = (Data3 * 256 + Data4) * 0.01
        // Note: vehicleSpeed is already in protocol format (km/h)
        uint16_t speedRaw = (uint16_t)vehicleSpeed;
        payload41[3] = (uint8_t)(speedRaw >> 8);   // Data3: MSB
        payload41[4] = (uint8_t)(speedRaw & 0xFF); // Data4: LSB
        
        // Bytes [5-6]: Battery voltage
        // Format: Big Endian unsigned 16-bit
        // Formula: Voltage (V) = (Data5 * 256 + Data6) * 0.01
        // Note: voltBat is already in protocol format (V)
        uint16_t vBatRaw = (uint16_t)voltBat;
        payload41[5] = (uint8_t)(vBatRaw >> 8);   // Data5: MSB
        payload41[6] = (uint8_t)(vBatRaw & 0xFF); // Data6: LSB
        
        // Bytes [7-8]: Outside temperature
        // Format: Big Endian signed 16-bit
        // Formula: Temperature (°C) = (Data7 * 256 + Data8) * 0.1
        // Note: tempExt is already in protocol format (°C)
        // Note: Using coolant temp as substitute (no dedicated exterior sensor)
        int16_t tRaw = (int16_t)tempExt;
        payload41[7] = (uint8_t)((tRaw >> 8) & 0xFF); // Data7: MSB (signed)
        payload41[8] = (uint8_t)(tRaw & 0xFF);        // Data8: LSB
        
        // Bytes [9-11]: Odometer
        // Format: Big Endian 24-bit unsigned
        // Formula: Odometer (km) = Data9 * 65536 + Data10 * 256 + Data11
        payload41[9]  = (uint8_t)((currentOdo >> 16) & 0xFF); // Data9: High byte
        payload41[10] = (uint8_t)((currentOdo >> 8) & 0xFF);   // Data10: Mid byte
        payload41[11] = (uint8_t)(currentOdo & 0xFF);          // Data11: Low byte
        
        // Byte [12]: Fuel level
        // Format: Unsigned 8-bit
        // Formula: Fuel (L) = Data12
        // Scale: Already mapped to 0-45L (Juke F15 tank capacity)
        payload41[12] = (uint8_t)fuelLevel;

        transmettreVersPoste(0x41, payload41, 13);
        lastSlowTime = now;
    }

    // ======================================================================
    // 3. DOOR STATUS (DataType 0x41, Command 0x01) - On change only
    // ======================================================================
    // Format: 2 bytes total
    // Protocol specification: Raise VW Polo Protocol v1.4
    // Byte [0]: Command = 0x01 (Doors/safety/trunk/washer)
    // Byte [1]: Status bitmask
    //   Bit 0: Left front door (0=closed, 1=open)
    //   Bit 1: Right front door (0=closed, 1=open)
    //   Bit 2: Left rear door (0=closed, 1=open)
    //   Bit 3: Rear door if present (0=closed, 1=open)
    //   Bit 4: Trunk status (0=closed, 1=open)
    //   Bit 5: Handbrake (0=normal/released, 1=applied)
    //   Bit 6: Washer fluid level (0=normal, 1=low) - not available from CAN
    //   Bit 7: Driver seat belt (0=normal/fastened, 1=not fastened) - not available from CAN
    static uint8_t lastSentDoors = 0xFF; 
    uint8_t doorStatus = 0;
    
    // Map internal format (currentDoors) to VW protocol format
    if (currentDoors & 0x80) doorStatus |= 0x01;  // Bit0: Front Left (Driver)
    if (currentDoors & 0x40) doorStatus |= 0x02;  // Bit1: Front Right
    if (currentDoors & 0x20) doorStatus |= 0x04;  // Bit2: Rear Left
    if (currentDoors & 0x10) doorStatus |= 0x08;  // Bit3: Rear Right (or rear door)
    if (currentDoors & 0x08) doorStatus |= 0x10;  // Bit4: Trunk
    if (currentDoors & 0x01) doorStatus |= 0x20;  // Bit5: Handbrake
    // Bit6: Washer fluid (not available from CAN, leave at 0)
    // Bit7: Seat belt (not available from CAN, leave at 0)

    // Only send when door or handbrake status changes
    if (doorStatus != lastSentDoors) {
        uint8_t payload[2] = {0x01, doorStatus};
        transmettreVersPoste(0x41, payload, 2);
        lastSentDoors = doorStatus;
    }

    // ======================================================================
    // 4. DEBUG LOGGING (Every 1 second when Serial is connected)
    // ======================================================================
    if (Serial && (now - lastLogRadio >= 1000)) {
        Serial.println(">>> RADIO TX STATUS >>>");
        Serial.printf("RPM: %u | Fuel: %u L | Odo: %u km\n", engineRPM, fuelLevel, currentOdo);
        Serial.printf("VW Steer (IPAS Target): %d | Door+HB Byte: 0x%02X\n", angleVW, doorStatus);
        Serial.printf("VW Bat Raw: %u | VW Temp Raw: %d\n", (uint16_t)voltBat, (int16_t)tempExt);
        Serial.println("-----------------------");
        lastLogRadio = now;
    }
}