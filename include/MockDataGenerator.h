/**
 * @file MockDataGenerator.h
 * @brief Mock data generator for testing without a real vehicle
 *
 * This module generates simulated vehicle data for development and testing.
 * It allows the system to function without being connected to an actual
 * vehicle CAN bus, useful for UI testing and debugging.
 *
 * Features:
 * - Oscillating values that simulate realistic vehicle behavior
 * - Configurable update rate (default 20 Hz)
 * - Simulated indicator blinking
 * - Realistic value ranges based on actual vehicle data
 *
 * Usage:
 * MockDataGenerator is automatically activated when:
 * - No configuration file is found on SPIFFS
 * - Configuration file has "isMock": true
 *
 * The generator directly updates GlobalData variables, which are then
 * transmitted to the head unit by RadioSend, just like real CAN data.
 */

#ifndef MOCK_DATA_GENERATOR_H
#define MOCK_DATA_GENERATOR_H

#include <Arduino.h>
#include "VehicleConfig.h"

// =============================================================================
// MOCK CONFIGURATION STRUCTURES
// =============================================================================

/**
 * @brief Defines realistic bounds for a mock data field
 *
 * Each field has a defined range and behavior:
 * - Static values (cycleStep = 0): Fuel level, DTE, average consumption
 * - Oscillating values: RPM, speed, steering (bounce between min/max)
 */
struct MockFieldBounds {
    OutputField field;          // Which field this defines
    int32_t minValue;           // Minimum realistic value
    int32_t maxValue;           // Maximum realistic value
    int32_t typicalValue;       // Starting/typical value
    int32_t cycleStep;          // Increment per update (0 = static)
};

// =============================================================================
// MOCK DATA GENERATOR CLASS
// =============================================================================

/**
 * @brief Generates simulated vehicle data for testing
 *
 * Creates realistic-looking vehicle telemetry without requiring
 * an actual CAN bus connection.
 */
class MockDataGenerator {
public:
    MockDataGenerator();

    /**
     * @brief Initialize mock generator with default bounds
     *
     * Sets up initial values and prepares oscillation state.
     * Should be called once during setup() if mock mode is active.
     */
    void begin();

    /**
     * @brief Update mock values and write to GlobalData
     *
     * Call this every loop iteration when in mock mode.
     * Respects the configured update interval to limit CPU usage.
     */
    void update();

    /**
     * @brief Set update interval in milliseconds
     * @param intervalMs Time between updates (default: 50ms = 20 Hz)
     */
    void setUpdateInterval(uint16_t intervalMs) { _updateInterval = intervalMs; }

private:
    // Timing
    unsigned long _lastUpdate;          // Timestamp of last update
    uint16_t _updateInterval;           // Milliseconds between updates

    // Value state arrays (indexed by OutputField enum)
    int32_t _currentValues[static_cast<int>(OutputField::FIELD_COUNT)];
    int8_t _directions[static_cast<int>(OutputField::FIELD_COUNT)];  // +1 or -1

    // Default mock bounds (defined in .cpp)
    static const MockFieldBounds _defaultBounds[];
    static const uint8_t _boundsCount;

    /**
     * @brief Write current mock values to GlobalData variables
     *
     * Transfers all simulated values to the shared global variables
     * that RadioSend reads from.
     */
    void writeToGlobalData();

    /**
     * @brief Get bounds configuration for a specific field
     * @param field OutputField to look up
     * @return Pointer to bounds, or nullptr if not configured
     */
    const MockFieldBounds* getBounds(OutputField field) const;
};

#endif // MOCK_DATA_GENERATOR_H
