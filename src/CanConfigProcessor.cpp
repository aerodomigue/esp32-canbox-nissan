/**
 * @file CanConfigProcessor.cpp
 * @brief Implementation of configurable CAN frame processor
 *
 * This module loads vehicle configuration from JSON files and uses it to
 * decode CAN frames into GlobalData variables. It provides flexibility to
 * support multiple vehicles without code changes.
 *
 * JSON Configuration Loading:
 * 1. Mount LittleFS filesystem
 * 2. Search for config files (/vehicle.json, /NissanJukeF15.json)
 * 3. Parse JSON using ArduinoJson
 * 4. Build internal VehicleProfile structure
 *
 * Frame Processing:
 * 1. Look up CAN ID in configuration
 * 2. For each field in the frame config:
 *    a. Extract raw bytes according to startByte, byteCount, byteOrder
 *    b. Apply conversion formula (SCALE, MAP_RANGE, BITMASK_EXTRACT)
 *    c. Write result to appropriate GlobalData variable
 */

#include "CanConfigProcessor.h"
#include "GlobalData.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

// =============================================================================
// CONSTRUCTOR
// =============================================================================

CanConfigProcessor::CanConfigProcessor()
    : _mockMode(true)           // Default to mock until config loaded
    , _framesProcessed(0)
    , _unknownFrames(0)
{
}

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * @brief Initialize processor and load configuration from SPIFFS
 *
 * Attempts to load configuration from known file paths.
 * If no config found, falls back to mock mode.
 */
bool CanConfigProcessor::begin() {
    // Initialize LittleFS filesystem
    if (!LittleFS.begin(true)) {
        Serial.println("[CanConfig] LittleFS mount failed");
        _mockMode = true;
        return false;
    }

    // Configuration file search order
    const char* configPaths[] = {
        "/vehicle.json",        // Primary: generic name for current vehicle
        "/NissanJukeF15.json"   // Fallback: specific vehicle config
    };

    // Try each config path until one succeeds
    for (const char* path : configPaths) {
        if (LittleFS.exists(path)) {
            Serial.printf("[CanConfig] Found config: %s\n", path);
            if (loadFromJson(path)) {
                // Use isMock flag from JSON (defaults to false if not specified)
                _mockMode = _profile.isMock;
                Serial.printf("[CanConfig] Loaded: %s (%d frames) - %s mode\n",
                              _profile.name.c_str(),
                              _profile.frames.size(),
                              _mockMode ? "MOCK" : "REAL CAN");
                return true;
            }
        }
    }

    // No config found - use mock mode with simulated data
    Serial.println("[CanConfig] No config found - MOCK mode active (default)");
    _mockMode = true;
    return false;
}

// =============================================================================
// JSON CONFIGURATION LOADING
// =============================================================================

/**
 * @brief Load and parse vehicle configuration from JSON file
 *
 * JSON Structure:
 * {
 *   "name": "Vehicle Name",
 *   "isMock": false,
 *   "frames": [
 *     { "canId": "0x180", "fields": [...] }
 *   ]
 * }
 */
bool CanConfigProcessor::loadFromJson(const char* path) {
    // Open configuration file
    File file = LittleFS.open(path, "r");
    if (!file) {
        Serial.printf("[CanConfig] Failed to open: %s\n", path);
        return false;
    }

    // Parse JSON document
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        Serial.printf("[CanConfig] JSON parse error: %s\n", error.c_str());
        return false;
    }

    // Clear any existing profile data
    _profile.frames.clear();
    _profile.name = doc["name"] | "Unknown";
    _profile.isMock = doc["isMock"] | false;  // Default to real CAN if not specified

    // Parse frames array
    JsonArray framesArray = doc["frames"];
    for (JsonObject frameObj : framesArray) {
        FrameConfig frame;

        // Parse CAN ID - supports both string ("0x180") and integer (384) formats
        const char* canIdStr = frameObj["canId"];
        if (canIdStr) {
            frame.canId = (uint16_t)strtol(canIdStr, nullptr, 0);  // Auto-detect base
        } else {
            frame.canId = frameObj["canId"].as<uint16_t>();
        }

        // Parse fields array for this frame
        JsonArray fieldsArray = frameObj["fields"];
        for (JsonObject fieldObj : fieldsArray) {
            FieldConfig field;

            // Target GlobalData field
            field.target = parseOutputField(fieldObj["target"] | "STEERING");

            // Byte extraction parameters
            field.startByte = fieldObj["startByte"] | 0;
            field.byteCount = fieldObj["byteCount"] | 1;
            field.byteOrder = parseByteOrder(fieldObj["byteOrder"] | "BE");
            field.dataType = parseDataType(fieldObj["dataType"] | "UINT8");

            // Conversion formula
            field.formula = parseFormulaType(fieldObj["formula"] | "NONE");

            // Initialize formula parameters to zero
            memset(field.params, 0, sizeof(field.params));

            // Parse formula parameters array [mult, div, offset] or [mask, shift]
            JsonArray paramsArray = fieldObj["params"];
            if (paramsArray) {
                int i = 0;
                for (JsonVariant v : paramsArray) {
                    if (i < 4) {
                        field.params[i++] = v.as<int32_t>();
                    }
                }
            }

            frame.fields.push_back(field);
        }

        _profile.frames.push_back(frame);
    }

    return _profile.frames.size() > 0;
}

// =============================================================================
// FRAME LOOKUP
// =============================================================================

/**
 * @brief Find frame configuration for a given CAN ID
 *
 * Linear search through configured frames. For small configs (< 20 frames)
 * this is efficient enough. Could be optimized with a lookup table if needed.
 */
const FrameConfig* CanConfigProcessor::findFrameConfig(uint16_t canId) const {
    for (const auto& frame : _profile.frames) {
        if (frame.canId == canId) {
            return &frame;
        }
    }
    return nullptr;
}

// =============================================================================
// FRAME PROCESSING
// =============================================================================

/**
 * @brief Process a received CAN frame according to configuration
 *
 * For each field defined for this CAN ID:
 * 1. Extract raw value from specified bytes
 * 2. Apply conversion formula
 * 3. Write to GlobalData
 */
bool CanConfigProcessor::processFrame(const CanFrame& frame) {
    // Look up configuration for this CAN ID
    const FrameConfig* config = findFrameConfig(frame.identifier);

    if (!config) {
        _unknownFrames++;
        return false;
    }

    _framesProcessed++;

    // Process each field defined for this frame
    for (const auto& field : config->fields) {
        // Step 1: Extract raw value from CAN data bytes
        int32_t rawValue = extractRawValue(frame.data, field);

        // Step 2: Apply conversion formula (SCALE, MAP_RANGE, etc.)
        int32_t convertedValue = applyFormula(rawValue, field);

        // Step 3: Write to appropriate GlobalData variable
        writeToGlobalData(field.target, convertedValue);
    }

    return true;
}

// =============================================================================
// VALUE EXTRACTION
// =============================================================================

/**
 * @brief Extract raw value from CAN frame data bytes
 *
 * Handles:
 * - Multi-byte values (1-4 bytes)
 * - Byte ordering (Big Endian / Little Endian)
 * - Signed/unsigned interpretation
 *
 * @example For Big Endian 2-byte value at offset 0:
 *          data[0]=0x12, data[1]=0x34 → result = 0x1234 = 4660
 */
int32_t CanConfigProcessor::extractRawValue(const uint8_t* data, const FieldConfig& field) const {
    uint32_t rawUnsigned = 0;

    if (field.byteOrder == ByteOrder::MSB_FIRST) {
        // Big Endian - Most Significant Byte first (common in automotive)
        for (uint8_t i = 0; i < field.byteCount; i++) {
            rawUnsigned = (rawUnsigned << 8) | data[field.startByte + i];
        }
    } else {
        // Little Endian - Least Significant Byte first
        for (int8_t i = field.byteCount - 1; i >= 0; i--) {
            rawUnsigned = (rawUnsigned << 8) | data[field.startByte + i];
        }
    }

    // Handle signed data types (sign extension)
    switch (field.dataType) {
        case DataType::INT8:
            return (int8_t)rawUnsigned;
        case DataType::INT16:
            return (int16_t)rawUnsigned;
        default:
            return (int32_t)rawUnsigned;
    }
}

// =============================================================================
// FORMULA APPLICATION
// =============================================================================

/**
 * @brief Apply conversion formula to transform raw CAN value
 *
 * Formula Types:
 * - NONE: Return raw value unchanged
 * - SCALE: (value * multiplier / divisor) + offset
 *          Example: RPM raw 17500 with params[1,7,0] → 2500
 * - MAP_RANGE: Arduino map() function
 *          Example: Fuel 255→0 mapped to 0→45 liters
 * - BITMASK_EXTRACT: (value & mask) >> shift
 *          Example: Extract bit 20 from 24-bit status word
 */
int32_t CanConfigProcessor::applyFormula(int32_t rawValue, const FieldConfig& field) const {
    switch (field.formula) {
        case FormulaType::NONE:
            return rawValue;

        case FormulaType::SCALE: {
            // params[0] = multiplier, params[1] = divisor, params[2] = offset
            int32_t mult = field.params[0] != 0 ? field.params[0] : 1;
            int32_t div = field.params[1] != 0 ? field.params[1] : 1;
            int32_t offset = field.params[2];
            return ((rawValue * mult) / div) + offset;
        }

        case FormulaType::MAP_RANGE: {
            // params[0] = inMin, params[1] = inMax, params[2] = outMin, params[3] = outMax
            return map(rawValue, field.params[0], field.params[1],
                       field.params[2], field.params[3]);
        }

        case FormulaType::BITMASK_EXTRACT: {
            // params[0] = bitmask, params[1] = right shift amount
            return (rawValue & field.params[0]) >> field.params[1];
        }

        default:
            return rawValue;
    }
}

// =============================================================================
// GLOBALDATA OUTPUT
// =============================================================================

/**
 * @brief Write converted value to the appropriate GlobalData variable
 *
 * Maps OutputField enum to global variables. Handles special cases:
 * - VOLTAGE: Converts from decivolts (141) to float (14.1)
 * - DOOR_*: Sets/clears bits in currentDoors bitmask
 * - INDICATOR_*: Updates timestamp for blink detection
 */
void CanConfigProcessor::writeToGlobalData(OutputField target, int32_t value) {
    switch (target) {
        // === Numeric Values ===
        case OutputField::STEERING:
            currentSteer = (int16_t)value;
            break;
        case OutputField::ENGINE_RPM:
            engineRPM = (uint16_t)value;
            break;
        case OutputField::VEHICLE_SPEED:
            vehicleSpeed = (uint8_t)value;
            break;
        case OutputField::FUEL_LEVEL:
            fuelLevel = (uint8_t)value;
            break;
        case OutputField::ODOMETER:
            currentOdo = (uint32_t)value;
            break;
        case OutputField::VOLTAGE:
            // Value is in decivolts (e.g., 141 = 14.1V), convert to float
            voltBat = value * 0.1f;
            break;
        case OutputField::TEMPERATURE:
            tempExt = (int8_t)value;
            break;
        case OutputField::DTE:
            dteValue = (int16_t)value;
            break;
        case OutputField::FUEL_CONS_INST:
            fuelConsumptionInst = (uint16_t)value;
            break;
        case OutputField::FUEL_CONS_AVG:
            fuelConsumptionAvg = (uint16_t)value;
            break;

        // === Door Status Flags ===
        // Map to currentDoors bitmask (Toyota RAV4 format)
        case OutputField::DOOR_DRIVER:
            if (value) currentDoors |= 0x80; else currentDoors &= ~0x80;
            break;
        case OutputField::DOOR_PASSENGER:
            if (value) currentDoors |= 0x40; else currentDoors &= ~0x40;
            break;
        case OutputField::DOOR_REAR_LEFT:
            if (value) currentDoors |= 0x20; else currentDoors &= ~0x20;
            break;
        case OutputField::DOOR_REAR_RIGHT:
            if (value) currentDoors |= 0x10; else currentDoors &= ~0x10;
            break;
        case OutputField::DOOR_BOOT:
            if (value) currentDoors |= 0x08; else currentDoors &= ~0x08;
            break;

        // === Turn Indicators ===
        // Update timestamp for blink detection (500ms timeout in RadioSend)
        case OutputField::INDICATOR_LEFT:
            if (value) lastLeftIndicatorTime = millis();
            break;
        case OutputField::INDICATOR_RIGHT:
            if (value) lastRightIndicatorTime = millis();
            break;

        // === Light Status ===
        case OutputField::HEADLIGHTS:
            headlightsOn = (value != 0);
            break;
        case OutputField::HIGH_BEAM:
            highBeamOn = (value != 0);
            break;
        case OutputField::PARKING_LIGHTS:
            parkingLightsOn = (value != 0);
            break;

        default:
            // Unknown field - ignore
            break;
    }
}
