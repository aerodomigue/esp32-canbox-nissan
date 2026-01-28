/**
 * @file RadioSend.cpp
 * @brief Toyota RAV4 protocol communication for Android head unit
 *
 * This module translates decoded Nissan CAN data into the Toyota RAV4 protocol
 * format expected by aftermarket Android head units. Communication occurs
 * over UART at 38400 baud, 8N1 format.
 *
 * Protocol: Raise Toyota RAV4 2019-2020
 * Reference: RAV4_BODY_ENGINE_2018_2020.pdf
 *
 * Frame Format:
 * ┌──────┬─────────┬────────┬──────────────┬──────────┐
 * │ 0x2E │ Command │ Length │ Payload[n]   │ Checksum │
 * │ HEAD │  (1B)   │  (1B)  │ (Length B)   │   (1B)   │
 * └──────┴─────────┴────────┴──────────────┴──────────┘
 *
 * Checksum = (Command + Length + Data0 + ... + DataN) XOR 0xFF
 *
 * Commands (SLAVE → HOST):
 * - 0x21: Remaining range / Distance to empty
 * - 0x24: Door status (1 byte bitmask)
 * - 0x28: Outside temperature (12 bytes, temp at [5])
 * - 0x29: Steering wheel angle (2 bytes LE, 0.1° units)
 * - 0x7D: Multi-function (sub-commands: 0x03=speed, 0x04=odo, 0x0A=rpm)
 */

#include <Arduino.h>
#include "GlobalData.h"
#include "ConfigManager.h"

extern HardwareSerial RadioSerial;

// =============================================================================
// TOYOTA RAV4 PROTOCOL COMMANDS
// =============================================================================
const uint8_t CMD_REMAINING_RANGE    = 0x21;
const uint8_t CMD_FUEL_CONSUMPTION   = 0x22;  // Instantaneous
const uint8_t CMD_FUEL_CONS_AVG      = 0x23;  // Average/History
const uint8_t CMD_DOOR_STATUS        = 0x24;
const uint8_t CMD_OUTSIDE_TEMP       = 0x28;
const uint8_t CMD_STEERING_WHEEL     = 0x29;
const uint8_t CMD_MULTI_FUNCTION     = 0x7D;

// Fuel consumption unit
const uint8_t FUEL_UNIT_L100KM = 0x02;

// Multi-function sub-commands
const uint8_t SUBCMD_LIGHTS   = 0x01;
const uint8_t SUBCMD_SPEED    = 0x03;
const uint8_t SUBCMD_ODOMETER = 0x04;
const uint8_t SUBCMD_RPM      = 0x0A;

// Lights bitmask (Toyota RAV4 format)
const uint8_t MASK_LIGHT_RIGHT_IND   = 0x08;  // Right indicator
const uint8_t MASK_LIGHT_LEFT_IND    = 0x10;  // Left indicator
const uint8_t MASK_LIGHT_HIGH_BEAM   = 0x20;  // High beam
const uint8_t MASK_LIGHT_HEADLIGHTS  = 0x40;  // Headlights (low beam)
const uint8_t MASK_LIGHT_PARKING     = 0x80;  // Parking lights

// Indicator timeout loaded from ConfigManager

// =============================================================================
// SEND INTERVALS (milliseconds)
// =============================================================================
const unsigned long STEERING_INTERVAL_MS  = 200;   // Fast for camera guidelines
const unsigned long LIGHTS_INTERVAL_MS    = 200;   // Lights/indicators (fast for blink)
const unsigned long DOOR_INTERVAL_MS      = 250;   // Door/status updates
const unsigned long RPM_INTERVAL_MS       = 333;   // ~3 Hz for smooth gauge
const unsigned long SPEED_INTERVAL_MS     = 500;   // Speed updates
const unsigned long FUEL_CONS_INTERVAL_MS     = 1000;  // Instantaneous fuel consumption (1s)
const unsigned long FUEL_CONS_AVG_INTERVAL_MS = 5000;  // Average fuel consumption (5s)
const unsigned long TEMP_INTERVAL_MS          = 5000;  // Temperature (slow)
const unsigned long RANGE_INTERVAL_MS         = 5000;  // Trip info / remaining range (slow)
const unsigned long ODOMETER_INTERVAL_MS      = 10000; // Odometer (very slow)

// =============================================================================
// DOOR BITMASK (Toyota RAV4 format)
// =============================================================================
const uint8_t MASK_DOOR_DRIVER     = 0x80;  // Bit 7
const uint8_t MASK_DOOR_PASSENGER  = 0x40;  // Bit 6
const uint8_t MASK_DOOR_REAR_LEFT  = 0x10;  // Bit 4
const uint8_t MASK_DOOR_REAR_RIGHT = 0x20;  // Bit 5
const uint8_t MASK_DOOR_BOOT       = 0x08;  // Bit 3

// Timer variables
static unsigned long lastSteeringTime = 0;
static unsigned long lastLightsTime = 0;
static unsigned long lastDoorTime = 0;
static unsigned long lastRpmTime = 0;
static unsigned long lastSpeedTime = 0;
static unsigned long lastFuelConsTime = 0;
static unsigned long lastFuelConsAvgTime = 0;
static unsigned long lastTempTime = 0;
static unsigned long lastRangeTime = 0;
static unsigned long lastOdometerTime = 0;
static uint8_t lastSentDoors = 0xFF;
static uint8_t lastSentLights = 0xFF;

/**
 * @brief Transmit a data frame to the head unit
 * @param cmd Command byte
 * @param data Pointer to payload data array
 * @param len Number of bytes in payload
 */
void sendCanboxMessage(uint8_t cmd, const uint8_t* data, uint8_t len) {
    // Calculate checksum: (cmd + len + sum(data)) XOR 0xFF
    uint8_t sum = cmd + len;
    for (uint8_t i = 0; i < len; i++) {
        sum += data[i];
    }
    uint8_t checksum = sum ^ 0xFF;

    RadioSerial.write(0x2E);      // Header
    RadioSerial.write(cmd);       // Command
    RadioSerial.write(len);       // Length
    RadioSerial.write(data, len); // Payload
    RadioSerial.write(checksum);  // Checksum
}

/**
 * @brief Send door status
 * @param doorMask Bitmask of open doors
 */
void sendDoorCommand(uint8_t doorMask) {
    sendCanboxMessage(CMD_DOOR_STATUS, &doorMask, 1);
}

/**
 * @brief Send steering wheel angle
 * @param angle Signed 16-bit angle in 0.1° units, Little Endian
 *
 * Range: -5400 to +5400 (±540.0° at full lock)
 * Per PDF: "Steering wheel angle is -540 to 540"
 */
void sendSteeringAngleMessage(int16_t angle) {
    uint8_t payload[2] = {
        (uint8_t)(angle & 0xFF),        // LSB
        (uint8_t)((angle >> 8) & 0xFF)  // MSB
    };
    sendCanboxMessage(CMD_STEERING_WHEEL, payload, 2);
}

/**
 * @brief Send engine RPM via multi-function command
 * @param rpm Engine RPM value
 *
 * Encoding: RPM × 4, Little Endian
 */
void sendRpmMessage(uint16_t rpm) {
    uint16_t encoded = rpm * 4;
    uint8_t payload[3] = {
        SUBCMD_RPM,
        (uint8_t)(encoded & 0xFF),        // LSB
        (uint8_t)((encoded >> 8) & 0xFF)  // MSB
    };
    sendCanboxMessage(CMD_MULTI_FUNCTION, payload, 3);
}

/**
 * @brief Send vehicle speed via multi-function command
 * @param speed Speed in km/h
 *
 * Encoding: Speed × 100, Little Endian (0.01 km/h resolution)
 */
void sendSpeedMessage(uint16_t speed) {
    uint16_t encoded = speed * 100;
    uint8_t payload[5] = {
        SUBCMD_SPEED,
        (uint8_t)(encoded & 0xFF),        // Speed LSB
        (uint8_t)((encoded >> 8) & 0xFF), // Speed MSB
        0x00,  // Average speed LSB (not available)
        0x00   // Average speed MSB (not available)
    };
    sendCanboxMessage(CMD_MULTI_FUNCTION, payload, 5);
}

/**
 * @brief Send odometer value via multi-function command
 * @param odo Odometer in km (24-bit)
 *
 * Encoding: Little Endian 24-bit value
 * Per PDF: includes Trip 1/2 but we only have odometer
 */
void sendOdometerMessage(uint32_t odo) {
    uint8_t payload[12] = {
        SUBCMD_ODOMETER,
        (uint8_t)(odo & 0xFF),          // Odo LSB
        (uint8_t)((odo >> 8) & 0xFF),   // Odo Mid
        (uint8_t)((odo >> 16) & 0xFF),  // Odo MSB
        0xF2, 0x08,                     // Unknown/reserved (per PDF)
        0x00, 0x00, 0x00,               // Trip 1 (not available)
        0x00, 0x00, 0x00                // Trip 2 (not available)
    };
    sendCanboxMessage(CMD_MULTI_FUNCTION, payload, 12);
}

/**
 * @brief Send outside temperature
 * @param temp Temperature in °C
 *
 * Encoding: (temp + 40) × 2 at byte [5]
 * Per PDF: 12-byte payload, all zeros except byte 5
 */
void sendOutsideTempMessage(int8_t temp) {
    uint8_t encoded = (uint8_t)((temp + 40) * 2);
    uint8_t payload[12] = {0};
    payload[5] = encoded;
    sendCanboxMessage(CMD_OUTSIDE_TEMP, payload, 12);
}

/**
 * @brief Send trip info with remaining range, average speed, and elapsed time
 * @param range_km Remaining range in km
 * @param avg_speed_01 Average speed in 0.1 km/h units (e.g., 450 = 45.0 km/h)
 * @param elapsed_sec Elapsed driving time in seconds
 *
 * Per Chinese protocol document (Toyota RAV4):
 * - Cmd = 0x21, Len = 0x07
 * - Data0-1: Average Speed (Big Endian, 0.1 km/h units)
 * - Data2-3: Elapsed Time (Big Endian, seconds)
 * - Data4-5: Cruising Range (Big Endian, km)
 * - Data6: Unit (0x02 = km)
 */
void sendTripInfoMessage(uint16_t range_km, uint16_t avg_speed_01, uint16_t elapsed_sec) {
    uint8_t payload[7] = {
        (uint8_t)((avg_speed_01 >> 8) & 0xFF),  // Average Speed MSB
        (uint8_t)(avg_speed_01 & 0xFF),          // Average Speed LSB
        (uint8_t)((elapsed_sec >> 8) & 0xFF),    // Elapsed Time MSB
        (uint8_t)(elapsed_sec & 0xFF),           // Elapsed Time LSB
        (uint8_t)((range_km >> 8) & 0xFF),       // Range MSB
        (uint8_t)(range_km & 0xFF),              // Range LSB
        0x02                                      // Unit: km
    };
    sendCanboxMessage(CMD_REMAINING_RANGE, payload, 7);
}

/**
 * @brief Send instantaneous fuel consumption
 * @param consumption_01 Consumption in 0.1 L/100km units (e.g., 75 = 7.5 L/100km)
 *
 * Per Chinese protocol document (Toyota RAV4):
 * - Cmd = 0x22, Len = 0x03
 * - Data0: Unit (0x02 = L/100km)
 * - Data1-2: Value (Big Endian), divide by 10 to get L/100km
 */
void sendFuelConsumptionMessage(uint16_t consumption_01) {
    uint8_t payload[3] = {
        FUEL_UNIT_L100KM,                        // Unit: L/100km
        (uint8_t)((consumption_01 >> 8) & 0xFF), // Value MSB
        (uint8_t)(consumption_01 & 0xFF)         // Value LSB
    };
    sendCanboxMessage(CMD_FUEL_CONSUMPTION, payload, 3);
}

/**
 * @brief Send average/history fuel consumption
 * @param consumption_01 Average consumption in 0.1 L/100km units
 *
 * Per Chinese protocol document (Toyota RAV4):
 * - Cmd = 0x23, Len = 0x03
 * - Data0: Unit (0x02 = L/100km)
 * - Data1-2: Value (Big Endian), divide by 10 to get L/100km
 */
void sendFuelConsumptionAvgMessage(uint16_t consumption_01) {
    uint8_t payload[3] = {
        FUEL_UNIT_L100KM,                        // Unit: L/100km
        (uint8_t)((consumption_01 >> 8) & 0xFF), // Value MSB
        (uint8_t)(consumption_01 & 0xFF)         // Value LSB
    };
    sendCanboxMessage(CMD_FUEL_CONS_AVG, payload, 3);
}

/**
 * @brief Send lights and indicators status via multi-function command
 * @param lightMask Bitmask of active lights
 *
 * Per PDF Section 3:
 * - Cmd = 0x7D, Sub = 0x01, Len = 0x02
 * - Bitmask: 0x08=Right, 0x10=Left, 0x20=HighBeam, 0x40=Headlights, 0x80=Parking
 */
void sendLightsMessage(uint8_t lightMask) {
    uint8_t payload[2] = {
        SUBCMD_LIGHTS,
        lightMask
    };
    sendCanboxMessage(CMD_MULTI_FUNCTION, payload, 2);
}

/**
 * @brief Handle protocol handshake with the head unit (if needed)
 *
 * Toyota RAV4 protocol doesn't require explicit handshake,
 * but we still consume any incoming data.
 */
void handshake() {
    while (RadioSerial.available() > 0) {
        RadioSerial.read(); // Discard incoming data
    }
}

/**
 * @brief Main update function - sends all vehicle data to the radio
 *
 * Update intervals:
 * - Steering angle: 200ms (fast for camera guidelines)
 * - Door status: 250ms (or on change)
 * - Lights/indicators: 200ms (or on change)
 * - RPM: 333ms (~3 Hz)
 * - Speed: 500ms
 * - Fuel consumption: 1s
 * - Temperature: 5s
 * - Remaining range: 5s
 * - Odometer: 10s
 */
void processRadioUpdates() {
    unsigned long now = millis();

    handshake();

    // =========================================================================
    // 1. STEERING WHEEL ANGLE (CMD 0x29) - 200ms interval
    // =========================================================================
    // Per PDF: Angle in 0.1° units, range -540 to +540 (so -5400 to +5400 raw)
    // Calibration values from ConfigManager (stored in NVS)
    if (now - lastSteeringTime >= STEERING_INTERVAL_MS) {
        // Step 1: Apply center offset from config
        int32_t centered = (int32_t)currentSteer + configGetSteerOffset();

        // Step 2: Apply scale factor from config (percent, 100 = 1.0x)
        int32_t angleRAV4 = (centered * configGetSteerScale()) / 100;

        // Step 3: Invert direction if configured
        if (configGetSteerInvert()) {
            angleRAV4 = -angleRAV4;
        }

        sendSteeringAngleMessage(angleRAV4);
        lastSteeringTime = now;
    }

    // =========================================================================
    // 2. DOOR STATUS (CMD 0x24) - 250ms interval or on change
    // =========================================================================
    uint8_t doorStatus = 0;

    if (currentDoors & 0x80) doorStatus |= MASK_DOOR_DRIVER;     // Front Left
    if (currentDoors & 0x40) doorStatus |= MASK_DOOR_PASSENGER;  // Front Right
    if (currentDoors & 0x20) doorStatus |= MASK_DOOR_REAR_LEFT;  // Rear Left
    if (currentDoors & 0x10) doorStatus |= MASK_DOOR_REAR_RIGHT; // Rear Right
    if (currentDoors & 0x08) doorStatus |= MASK_DOOR_BOOT;       // Trunk

    if (doorStatus != lastSentDoors || (now - lastDoorTime >= DOOR_INTERVAL_MS)) {
        sendDoorCommand(doorStatus);
        lastSentDoors = doorStatus;
        lastDoorTime = now;
    }

    // =========================================================================
    // 3. LIGHTS & INDICATORS (CMD 0x7D, SUB 0x01) - 200ms interval or on change
    // =========================================================================
    // Build lights bitmask from global state
    // Indicators use timeout detection (configurable) since CAN only signals when active
    uint8_t lightStatus = 0;

    // Check if indicators are active (received signal within timeout)
    uint16_t indTimeout = configGetIndicatorTimeout();
    bool leftActive = (now - lastLeftIndicatorTime) < indTimeout;
    bool rightActive = (now - lastRightIndicatorTime) < indTimeout;

    if (rightActive)      lightStatus |= MASK_LIGHT_RIGHT_IND;
    if (leftActive)       lightStatus |= MASK_LIGHT_LEFT_IND;
    if (highBeamOn)       lightStatus |= MASK_LIGHT_HIGH_BEAM;
    if (headlightsOn)     lightStatus |= MASK_LIGHT_HEADLIGHTS;
    if (parkingLightsOn)  lightStatus |= MASK_LIGHT_PARKING;

    if (lightStatus != lastSentLights || (now - lastLightsTime >= LIGHTS_INTERVAL_MS)) {
        sendLightsMessage(lightStatus);
        lastSentLights = lightStatus;
        lastLightsTime = now;
    }

    // =========================================================================
    // 4. ENGINE RPM (CMD 0x7D, SUB 0x0A) - 333ms interval
    // =========================================================================
    if (now - lastRpmTime >= RPM_INTERVAL_MS) {
        sendRpmMessage(engineRPM);
        lastRpmTime = now;
    }

    // =========================================================================
    // 5. VEHICLE SPEED (CMD 0x7D, SUB 0x03) - 500ms interval
    // =========================================================================
    if (now - lastSpeedTime >= SPEED_INTERVAL_MS) {
        sendSpeedMessage(vehicleSpeed);
        lastSpeedTime = now;
    }

    // =========================================================================
    // 6. INSTANTANEOUS FUEL CONSUMPTION (CMD 0x22) - 1s interval
    // =========================================================================
    // Instantaneous consumption from Nissan CAN 0x580 byte[1]
    if (now - lastFuelConsTime >= FUEL_CONS_INTERVAL_MS) {
        sendFuelConsumptionMessage(fuelConsumptionInst);
        lastFuelConsTime = now;
    }

    // =========================================================================
    // 6b. AVERAGE FUEL CONSUMPTION (CMD 0x23) - 5s interval
    // =========================================================================
    // Average consumption from Nissan CAN 0x580 byte[4]
    if (now - lastFuelConsAvgTime >= FUEL_CONS_AVG_INTERVAL_MS) {
        sendFuelConsumptionAvgMessage(fuelConsumptionAvg);
        lastFuelConsAvgTime = now;
    }

    // =========================================================================
    // 7. OUTSIDE TEMPERATURE (CMD 0x28) - 5s interval
    // =========================================================================
    // Note: Using coolant temp as substitute (no exterior sensor on Juke CAN)
    if (now - lastTempTime >= TEMP_INTERVAL_MS) {
        sendOutsideTempMessage(tempExt);
        lastTempTime = now;
    }

    // =========================================================================
    // 8. TRIP INFO / REMAINING RANGE (CMD 0x21) - 5s interval
    // =========================================================================
    // Contains: Average Speed, Elapsed Time, Distance to Empty
    // Average speed/elapsed time from trip computer (if available on CAN)
    // Distance to Empty from Nissan CAN 0x54C
    if (now - lastRangeTime >= RANGE_INTERVAL_MS) {
        sendTripInfoMessage(dteValue, averageSpeed, elapsedTime);
        lastRangeTime = now;
    }

    // =========================================================================
    // 9. ODOMETER (CMD 0x7D, SUB 0x04) - 10s interval
    // =========================================================================
    if (now - lastOdometerTime >= ODOMETER_INTERVAL_MS) {
        sendOdometerMessage(currentOdo);
        lastOdometerTime = now;
    }
}
