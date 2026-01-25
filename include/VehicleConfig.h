/**
 * @file VehicleConfig.h
 * @brief CAN configuration structures for vehicle profiles
 *
 * This header defines the data structures used to describe CAN frame parsing
 * and data conversion. Configurations can be loaded from JSON files at runtime,
 * allowing support for multiple vehicles without code changes.
 *
 * Configuration JSON Format:
 * ┌─────────────────────────────────────────────────────────────────┐
 * │ {                                                               │
 * │   "name": "Vehicle Name",                                       │
 * │   "isMock": false,         // true = simulate data              │
 * │   "frames": [                                                   │
 * │     {                                                           │
 * │       "canId": "0x180",                                         │
 * │       "fields": [                                               │
 * │         {                                                       │
 * │           "target": "ENGINE_RPM",                               │
 * │           "startByte": 0, "byteCount": 2,                       │
 * │           "byteOrder": "BE", "dataType": "UINT16",              │
 * │           "formula": "SCALE", "params": [1, 7, 0]               │
 * │         }                                                       │
 * │       ]                                                         │
 * │     }                                                           │
 * │   ]                                                             │
 * │ }                                                               │
 * └─────────────────────────────────────────────────────────────────┘
 *
 * Supported Formulas:
 * - NONE:           Raw value, no conversion
 * - SCALE:          (value * mult / div) + offset  [params: mult, div, offset]
 * - MAP_RANGE:      map(value, inMin, inMax, outMin, outMax)
 * - BITMASK_EXTRACT: (value & mask) >> shift      [params: mask, shift]
 */

#ifndef VEHICLE_CONFIG_H
#define VEHICLE_CONFIG_H

#include <Arduino.h>
#include <vector>

// =============================================================================
// DATA TYPE DEFINITIONS
// =============================================================================

/**
 * @brief Supported data extraction types from CAN frames
 *
 * Determines how raw bytes are interpreted when extracted from the CAN frame.
 * Signed types (INT8, INT16) will properly handle negative values.
 */
enum class DataType : uint8_t {
    UINT8,          // Single byte unsigned (0-255)
    INT8,           // Single byte signed (-128 to 127)
    UINT16,         // 2 bytes unsigned (0-65535)
    INT16,          // 2 bytes signed (-32768 to 32767)
    UINT24,         // 3 bytes unsigned (odometer, etc.)
    UINT32,         // 4 bytes unsigned
    BITMASK         // Extract specific bits from multi-byte value
};

/**
 * @brief Byte ordering for multi-byte values
 *
 * Most automotive CAN buses use Big Endian (MSB first).
 * Note: Using MSB_FIRST/LSB_FIRST to avoid conflict with system macros.
 */
enum class ByteOrder : uint8_t {
    MSB_FIRST,      // Big Endian - Most Significant Byte first (Nissan standard)
    LSB_FIRST       // Little Endian - Least Significant Byte first
};

/**
 * @brief Formula types for data conversion
 *
 * After extracting the raw value, these formulas convert it to standard units.
 * Example: RPM raw value 17500 with SCALE(1, 7, 0) → 2500 RPM
 */
enum class FormulaType : uint8_t {
    NONE,           // No conversion - use raw value as-is
    SCALE,          // Linear scaling: (value * multiplier / divisor) + offset
    MAP_RANGE,      // Range mapping: map(value, inMin, inMax, outMin, outMax)
    BITMASK_EXTRACT // Bit extraction: (value & mask) >> shift
};

/**
 * @brief Target GlobalData field identifiers
 *
 * Maps extracted CAN data to the corresponding global variable.
 * These variables are then read by RadioSend to transmit to the head unit.
 */
enum class OutputField : uint8_t {
    // Numeric values
    STEERING,           // Steering wheel angle (0.1° units)
    ENGINE_RPM,         // Engine speed (RPM)
    VEHICLE_SPEED,      // Vehicle speed (km/h)
    FUEL_LEVEL,         // Fuel level (liters)
    ODOMETER,           // Total mileage (km)
    VOLTAGE,            // Battery voltage (0.1V units → converted to float)
    TEMPERATURE,        // External/coolant temperature (°C)
    DTE,                // Distance to empty (km)
    FUEL_CONS_INST,     // Instantaneous fuel consumption (0.1 L/100km)
    FUEL_CONS_AVG,      // Average fuel consumption (0.1 L/100km)

    // Door status flags (mapped to currentDoors bitmask)
    DOOR_DRIVER,        // Driver door open
    DOOR_PASSENGER,     // Passenger door open
    DOOR_REAR_LEFT,     // Rear left door open
    DOOR_REAR_RIGHT,    // Rear right door open
    DOOR_BOOT,          // Boot/trunk open

    // Light/indicator flags
    INDICATOR_LEFT,     // Left turn indicator active
    INDICATOR_RIGHT,    // Right turn indicator active
    HEADLIGHTS,         // Low beam headlights on
    HIGH_BEAM,          // High beam on
    PARKING_LIGHTS,     // Parking/position lights on

    // End marker for array sizing
    FIELD_COUNT
};

// =============================================================================
// CONFIGURATION STRUCTURES
// =============================================================================

/**
 * @brief Defines how to extract and convert a single data field from a CAN frame
 *
 * Each field represents one piece of data within a CAN frame. A single frame
 * can contain multiple fields (e.g., 0x5C5 contains both fuel level and odometer).
 */
struct FieldConfig {
    OutputField target;       // Which GlobalData field to update
    uint8_t     startByte;    // Starting byte index in CAN frame (0-7)
    uint8_t     byteCount;    // Number of bytes to extract (1-4)
    ByteOrder   byteOrder;    // Byte ordering for multi-byte values
    DataType    dataType;     // How to interpret the extracted bytes
    FormulaType formula;      // Conversion formula to apply
    int32_t     params[4];    // Formula parameters:
                              //   SCALE: [multiplier, divisor, offset, unused]
                              //   MAP_RANGE: [inMin, inMax, outMin, outMax]
                              //   BITMASK_EXTRACT: [mask, shift, unused, unused]
};

/**
 * @brief Defines a CAN frame and all its extractable fields
 *
 * Groups all data fields that share the same CAN identifier.
 */
struct FrameConfig {
    uint16_t canId;                    // CAN identifier (11-bit standard, 0x000-0x7FF)
    std::vector<FieldConfig> fields;   // Fields to extract from this frame
};

/**
 * @brief Complete vehicle configuration profile
 *
 * Represents all CAN frames and fields for a specific vehicle.
 * Loaded from JSON file on SPIFFS at startup.
 */
struct VehicleProfile {
    String name;                       // Vehicle name for logging/identification
    bool isMock;                       // true = mock mode (generate simulated data)
                                       // false = real CAN mode (read from bus)
    std::vector<FrameConfig> frames;   // All CAN frames to process
};

// =============================================================================
// STRING PARSING HELPERS
// =============================================================================
// These functions convert JSON string values to their enum equivalents.

/**
 * @brief Convert string to DataType enum
 * @param str String from JSON (e.g., "UINT16", "INT8")
 * @return Corresponding DataType enum value
 */
inline DataType parseDataType(const char* str) {
    if (strcmp(str, "UINT8") == 0) return DataType::UINT8;
    if (strcmp(str, "INT8") == 0) return DataType::INT8;
    if (strcmp(str, "UINT16") == 0) return DataType::UINT16;
    if (strcmp(str, "INT16") == 0) return DataType::INT16;
    if (strcmp(str, "UINT24") == 0) return DataType::UINT24;
    if (strcmp(str, "UINT32") == 0) return DataType::UINT32;
    if (strcmp(str, "BITMASK") == 0) return DataType::BITMASK;
    return DataType::UINT8; // Default fallback
}

/**
 * @brief Convert string to ByteOrder enum
 * @param str String from JSON (e.g., "BE", "LE", "BIG_ENDIAN")
 * @return Corresponding ByteOrder enum value
 */
inline ByteOrder parseByteOrder(const char* str) {
    if (strcmp(str, "LE") == 0 || strcmp(str, "LITTLE_ENDIAN") == 0 || strcmp(str, "LSB_FIRST") == 0) {
        return ByteOrder::LSB_FIRST;
    }
    return ByteOrder::MSB_FIRST; // Default: Big Endian (most common in automotive)
}

/**
 * @brief Convert string to FormulaType enum
 * @param str String from JSON (e.g., "SCALE", "MAP_RANGE")
 * @return Corresponding FormulaType enum value
 */
inline FormulaType parseFormulaType(const char* str) {
    if (strcmp(str, "NONE") == 0) return FormulaType::NONE;
    if (strcmp(str, "SCALE") == 0) return FormulaType::SCALE;
    if (strcmp(str, "MAP_RANGE") == 0) return FormulaType::MAP_RANGE;
    if (strcmp(str, "BITMASK_EXTRACT") == 0) return FormulaType::BITMASK_EXTRACT;
    return FormulaType::NONE; // Default: no conversion
}

/**
 * @brief Convert string to OutputField enum
 * @param str String from JSON (e.g., "ENGINE_RPM", "VEHICLE_SPEED")
 * @return Corresponding OutputField enum value
 */
inline OutputField parseOutputField(const char* str) {
    // Numeric values
    if (strcmp(str, "STEERING") == 0) return OutputField::STEERING;
    if (strcmp(str, "ENGINE_RPM") == 0) return OutputField::ENGINE_RPM;
    if (strcmp(str, "VEHICLE_SPEED") == 0) return OutputField::VEHICLE_SPEED;
    if (strcmp(str, "FUEL_LEVEL") == 0) return OutputField::FUEL_LEVEL;
    if (strcmp(str, "ODOMETER") == 0) return OutputField::ODOMETER;
    if (strcmp(str, "VOLTAGE") == 0) return OutputField::VOLTAGE;
    if (strcmp(str, "TEMPERATURE") == 0) return OutputField::TEMPERATURE;
    if (strcmp(str, "DTE") == 0) return OutputField::DTE;
    if (strcmp(str, "FUEL_CONS_INST") == 0) return OutputField::FUEL_CONS_INST;
    if (strcmp(str, "FUEL_CONS_AVG") == 0) return OutputField::FUEL_CONS_AVG;

    // Door flags
    if (strcmp(str, "DOOR_DRIVER") == 0) return OutputField::DOOR_DRIVER;
    if (strcmp(str, "DOOR_PASSENGER") == 0) return OutputField::DOOR_PASSENGER;
    if (strcmp(str, "DOOR_REAR_LEFT") == 0) return OutputField::DOOR_REAR_LEFT;
    if (strcmp(str, "DOOR_REAR_RIGHT") == 0) return OutputField::DOOR_REAR_RIGHT;
    if (strcmp(str, "DOOR_BOOT") == 0) return OutputField::DOOR_BOOT;

    // Light/indicator flags
    if (strcmp(str, "INDICATOR_LEFT") == 0) return OutputField::INDICATOR_LEFT;
    if (strcmp(str, "INDICATOR_RIGHT") == 0) return OutputField::INDICATOR_RIGHT;
    if (strcmp(str, "HEADLIGHTS") == 0) return OutputField::HEADLIGHTS;
    if (strcmp(str, "HIGH_BEAM") == 0) return OutputField::HIGH_BEAM;
    if (strcmp(str, "PARKING_LIGHTS") == 0) return OutputField::PARKING_LIGHTS;

    return OutputField::STEERING; // Default fallback
}

#endif // VEHICLE_CONFIG_H
