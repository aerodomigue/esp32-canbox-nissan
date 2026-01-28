/**
 * @file MockDataGenerator.cpp
 * @brief Implementation of mock data generator
 *
 * Generates simulated vehicle telemetry data for testing and development.
 * Values oscillate between realistic bounds to simulate driving conditions.
 *
 * Value Behaviors:
 * - OSCILLATING: Values bounce between min/max (RPM, speed, steering)
 * - STATIC: Values remain constant (fuel level, DTE, average consumption)
 * - BLINKING: Indicators toggle on/off (left turn signal)
 *
 * Update Rate: Default 20 Hz (50ms interval), configurable via setUpdateInterval()
 */

#include "MockDataGenerator.h"
#include "GlobalData.h"

// =============================================================================
// DEFAULT MOCK BOUNDS
// =============================================================================
// Defines realistic ranges for simulated values based on actual Nissan Juke data

const MockFieldBounds MockDataGenerator::_defaultBounds[] = {
    // Field                        Min      Max      Typical   Step
    // -------------------------------------------------------------------------
    // Oscillating values (simulate driving)
    { OutputField::STEERING,        -5400,   5400,    0,        100  },  // 0.1° units
    { OutputField::ENGINE_RPM,      800,     6000,    2500,     50   },  // RPM
    { OutputField::VEHICLE_SPEED,   0,       120,     60,       2    },  // km/h
    { OutputField::VOLTAGE,         125,     145,     140,      1    },  // 0.1V (12.5-14.5V)
    { OutputField::TEMPERATURE,     70,      95,      85,       1    },  // °C (coolant)
    { OutputField::FUEL_CONS_INST,  30,      120,     65,       3    },  // 0.1 L/100km
    { OutputField::ODOMETER,        85000,   85100,   85050,    1    },  // km (slow increment)

    // Static values (don't change during simulation)
    { OutputField::FUEL_LEVEL,      10,      45,      30,       0    },  // Liters
    { OutputField::DTE,             200,     400,     350,      0    },  // km range
    { OutputField::FUEL_CONS_AVG,   55,      75,      65,       0    },  // 0.1 L/100km
};

const uint8_t MockDataGenerator::_boundsCount = sizeof(_defaultBounds) / sizeof(_defaultBounds[0]);

// =============================================================================
// CONSTRUCTOR
// =============================================================================

MockDataGenerator::MockDataGenerator()
    : _lastUpdate(0)
    , _updateInterval(50)  // 20 Hz default update rate
{
    // Initialize arrays to zero
    memset(_currentValues, 0, sizeof(_currentValues));
    memset(_directions, 1, sizeof(_directions));  // Start with upward direction
}

// =============================================================================
// INITIALIZATION
// =============================================================================

/**
 * @brief Initialize mock generator with default values
 *
 * Sets all configured fields to their typical values and prepares
 * the oscillation state. Also initializes door/light states.
 */
void MockDataGenerator::begin() {
    Serial.println("[MockGen] Initializing mock data generator");

    // Initialize numeric values to their typical (starting) values
    for (uint8_t i = 0; i < _boundsCount; i++) {
        int fieldIndex = static_cast<int>(_defaultBounds[i].field);
        _currentValues[fieldIndex] = _defaultBounds[i].typicalValue;
        _directions[fieldIndex] = 1;  // Start oscillating upward
    }

    // Initialize door states (all closed)
    _currentValues[static_cast<int>(OutputField::DOOR_DRIVER)] = 0;
    _currentValues[static_cast<int>(OutputField::DOOR_PASSENGER)] = 0;
    _currentValues[static_cast<int>(OutputField::DOOR_REAR_LEFT)] = 0;
    _currentValues[static_cast<int>(OutputField::DOOR_REAR_RIGHT)] = 0;
    _currentValues[static_cast<int>(OutputField::DOOR_BOOT)] = 0;

    // Initialize light states
    _currentValues[static_cast<int>(OutputField::INDICATOR_LEFT)] = 0;
    _currentValues[static_cast<int>(OutputField::INDICATOR_RIGHT)] = 0;
    _currentValues[static_cast<int>(OutputField::HEADLIGHTS)] = 1;      // Headlights on
    _currentValues[static_cast<int>(OutputField::HIGH_BEAM)] = 0;
    _currentValues[static_cast<int>(OutputField::PARKING_LIGHTS)] = 0;

    // Write initial values to GlobalData
    writeToGlobalData();

    Serial.println("[MockGen] Mock data ready");
}

// =============================================================================
// UPDATE LOOP
// =============================================================================

/**
 * @brief Update mock values (call every loop iteration)
 *
 * Respects update interval to limit CPU usage.
 * For oscillating values, moves toward min or max and reverses at boundaries.
 * Also handles indicator blinking simulation.
 */
void MockDataGenerator::update() {
    unsigned long now = millis();

    // Respect update interval
    if (now - _lastUpdate < _updateInterval) {
        return;
    }
    _lastUpdate = now;

    // Update oscillating numeric values
    for (uint8_t i = 0; i < _boundsCount; i++) {
        const MockFieldBounds& bounds = _defaultBounds[i];
        int fieldIndex = static_cast<int>(bounds.field);

        // Skip static values (cycleStep = 0)
        if (bounds.cycleStep == 0) {
            continue;
        }

        // Get current value and direction
        int32_t& value = _currentValues[fieldIndex];
        int8_t& direction = _directions[fieldIndex];

        // Move value in current direction
        value += bounds.cycleStep * direction;

        // Bounce off boundaries
        if (value >= bounds.maxValue) {
            value = bounds.maxValue;
            direction = -1;  // Reverse to go down
        } else if (value <= bounds.minValue) {
            value = bounds.minValue;
            direction = 1;   // Reverse to go up
        }
    }

    // Simulate left indicator blinking (toggle every ~500ms)
    static unsigned long lastIndicatorToggle = 0;
    if (now - lastIndicatorToggle > 500) {
        lastIndicatorToggle = now;
        int leftIdx = static_cast<int>(OutputField::INDICATOR_LEFT);
        _currentValues[leftIdx] = !_currentValues[leftIdx];
    }

    // Write updated values to GlobalData
    writeToGlobalData();
}

// =============================================================================
// GLOBALDATA OUTPUT
// =============================================================================

/**
 * @brief Transfer mock values to GlobalData variables
 *
 * Copies all simulated values to the shared global variables.
 * RadioSend reads these to transmit to the head unit.
 */
void MockDataGenerator::writeToGlobalData() {
    // === Numeric Values ===
    currentSteer = (int16_t)_currentValues[static_cast<int>(OutputField::STEERING)];
    engineRPM = (uint16_t)_currentValues[static_cast<int>(OutputField::ENGINE_RPM)];
    vehicleSpeed = (uint8_t)_currentValues[static_cast<int>(OutputField::VEHICLE_SPEED)];
    fuelLevel = (uint8_t)_currentValues[static_cast<int>(OutputField::FUEL_LEVEL)];
    currentOdo = (uint32_t)_currentValues[static_cast<int>(OutputField::ODOMETER)];
    voltBat = _currentValues[static_cast<int>(OutputField::VOLTAGE)] * 0.1f;  // Convert to volts
    tempExt = (int8_t)_currentValues[static_cast<int>(OutputField::TEMPERATURE)];
    dteValue = (int16_t)_currentValues[static_cast<int>(OutputField::DTE)];
    fuelConsumptionInst = (uint16_t)_currentValues[static_cast<int>(OutputField::FUEL_CONS_INST)];
    fuelConsumptionAvg = (uint16_t)_currentValues[static_cast<int>(OutputField::FUEL_CONS_AVG)];

    // === Door Bitmask ===
    // Build Toyota RAV4 format door bitmask from individual flags
    currentDoors = 0;
    if (_currentValues[static_cast<int>(OutputField::DOOR_DRIVER)]) currentDoors |= 0x80;
    if (_currentValues[static_cast<int>(OutputField::DOOR_PASSENGER)]) currentDoors |= 0x40;
    if (_currentValues[static_cast<int>(OutputField::DOOR_REAR_LEFT)]) currentDoors |= 0x20;
    if (_currentValues[static_cast<int>(OutputField::DOOR_REAR_RIGHT)]) currentDoors |= 0x10;
    if (_currentValues[static_cast<int>(OutputField::DOOR_BOOT)]) currentDoors |= 0x08;

    // === Turn Indicators ===
    // Update timestamps for blink detection in RadioSend
    if (_currentValues[static_cast<int>(OutputField::INDICATOR_LEFT)]) {
        lastLeftIndicatorTime = millis();
    }
    if (_currentValues[static_cast<int>(OutputField::INDICATOR_RIGHT)]) {
        lastRightIndicatorTime = millis();
    }

    // === Light Status ===
    headlightsOn = _currentValues[static_cast<int>(OutputField::HEADLIGHTS)] != 0;
    highBeamOn = _currentValues[static_cast<int>(OutputField::HIGH_BEAM)] != 0;
    parkingLightsOn = _currentValues[static_cast<int>(OutputField::PARKING_LIGHTS)] != 0;
}

// =============================================================================
// HELPER FUNCTIONS
// =============================================================================

/**
 * @brief Look up bounds configuration for a specific field
 * @param field OutputField enum value
 * @return Pointer to MockFieldBounds if found, nullptr otherwise
 */
const MockFieldBounds* MockDataGenerator::getBounds(OutputField field) const {
    for (uint8_t i = 0; i < _boundsCount; i++) {
        if (_defaultBounds[i].field == field) {
            return &_defaultBounds[i];
        }
    }
    return nullptr;
}
