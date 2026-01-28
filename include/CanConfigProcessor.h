/**
 * @file CanConfigProcessor.h
 * @brief Configurable CAN frame processor
 *
 * This module processes incoming CAN frames based on vehicle configuration
 * loaded from JSON files on SPIFFS. It replaces the hardcoded CAN parsing
 * logic with a flexible, data-driven approach.
 *
 * Operating Modes:
 * - REAL CAN MODE: Reads actual CAN frames from the vehicle bus
 *   (JSON with "isMock": false)
 * - MOCK MODE: Generates simulated data for testing without a vehicle
 *   (JSON with "isMock": true, or no config file found)
 *
 * Data Flow:
 * ┌─────────────┐    ┌──────────────────┐    ┌────────────┐
 * │  CAN Bus    │───→│ CanConfigProcessor│───→│ GlobalData │
 * │  (TWAI)     │    │  (JSON config)   │    │ (shared)   │
 * └─────────────┘    └──────────────────┘    └────────────┘
 *                              ↑
 *                    ┌─────────────────┐
 *                    │ /vehicle.json   │
 *                    │ (LittleFS)      │
 *                    └─────────────────┘
 */

#ifndef CAN_CONFIG_PROCESSOR_H
#define CAN_CONFIG_PROCESSOR_H

#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include "VehicleConfig.h"

/**
 * @brief Configurable CAN frame processor
 *
 * Processes CAN frames using configuration loaded from JSON files.
 * Supports multiple vehicles by simply changing the config file.
 */
class CanConfigProcessor {
public:
    CanConfigProcessor();

    /**
     * @brief Initialize processor - load config from SPIFFS
     *
     * Searches for configuration files in order:
     * 1. /vehicle.json (primary)
     * 2. /NissanJukeF15.json (fallback)
     *
     * If no config found, activates mock mode with default simulated data.
     *
     * @return true if config loaded successfully, false if using mock mode
     */
    bool begin();

    /**
     * @brief Load vehicle configuration from JSON file
     * @param path Path to JSON file on SPIFFS (e.g., "/vehicle.json")
     * @return true if loaded and parsed successfully
     */
    bool loadFromJson(const char* path);

    /**
     * @brief Process a received CAN frame
     *
     * Looks up the frame's CAN ID in the configuration, extracts all defined
     * fields, applies conversion formulas, and writes to GlobalData.
     *
     * @param frame Reference to received CAN frame from TWAI
     * @return true if frame was handled (CAN ID found in config)
     */
    bool processFrame(const CanFrame& frame);

    /**
     * @brief Check if running in mock mode
     *
     * Mock mode is active when:
     * - No config file was found on SPIFFS
     * - Config file has "isMock": true
     *
     * @return true if generating simulated data
     */
    bool isMockMode() const { return _mockMode; }

    /**
     * @brief Get loaded profile name
     * @return Vehicle name from config, or "Unknown" if not loaded
     */
    const char* getProfileName() const { return _profile.name.c_str(); }

    /**
     * @brief Get count of successfully processed frames
     * @return Number of CAN frames that matched config and were processed
     */
    uint32_t getFramesProcessed() const { return _framesProcessed; }

    /**
     * @brief Get count of unrecognized frames
     * @return Number of CAN frames with IDs not in config
     */
    uint32_t getUnknownFrames() const { return _unknownFrames; }

private:
    VehicleProfile _profile;        // Loaded vehicle configuration
    bool _mockMode;                 // true = simulating data, false = real CAN
    uint32_t _framesProcessed;      // Statistics: processed frame count
    uint32_t _unknownFrames;        // Statistics: unknown CAN ID count

    /**
     * @brief Find frame configuration for a CAN ID
     * @param canId CAN identifier to search for
     * @return Pointer to FrameConfig if found, nullptr otherwise
     */
    const FrameConfig* findFrameConfig(uint16_t canId) const;

    /**
     * @brief Extract raw value from CAN frame bytes
     *
     * Handles byte ordering and data type interpretation.
     *
     * @param data Pointer to CAN frame data bytes
     * @param field Field configuration defining extraction parameters
     * @return Extracted raw value (before formula conversion)
     */
    int32_t extractRawValue(const uint8_t* data, const FieldConfig& field) const;

    /**
     * @brief Apply conversion formula to raw value
     *
     * Supports SCALE, MAP_RANGE, and BITMASK_EXTRACT formulas.
     *
     * @param rawValue Value extracted from CAN frame
     * @param field Field configuration with formula and parameters
     * @return Converted value in standard units
     */
    int32_t applyFormula(int32_t rawValue, const FieldConfig& field) const;

    /**
     * @brief Write converted value to GlobalData variable
     *
     * Maps OutputField enum to the corresponding global variable.
     * Handles special cases like door bitmasks and indicator timestamps.
     *
     * @param target Which GlobalData field to update
     * @param value Converted value to write
     */
    void writeToGlobalData(OutputField target, int32_t value);
};

#endif // CAN_CONFIG_PROCESSOR_H
