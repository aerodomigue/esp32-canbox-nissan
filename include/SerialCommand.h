/**
 * @file SerialCommand.h
 * @brief Serial command parser for USB configuration interface
 *
 * Provides a simple AT-style command interface for:
 * - Reading/writing calibration parameters (CFG)
 * - CAN configuration file management (CAN)
 * - OTA firmware updates (OTA)
 * - Enabling/disabling CAN debug logs (LOG)
 * - System information and control (SYS)
 *
 * Commands are newline-terminated and case-insensitive.
 *
 * CAN Config Upload Protocol:
 * 1. Host sends: CAN UPLOAD START <filename> <size_bytes>
 * 2. Device responds: OK READY
 * 3. Host sends: CAN UPLOAD DATA <base64_chunk> (multiple times)
 * 4. Device responds: OK <bytes_received>/<total_bytes>
 * 5. Host sends: CAN UPLOAD END
 * 6. Device validates JSON and responds: OK or ERROR
 *
 * OTA Firmware Update Protocol:
 * 1. Host sends: OTA START <size_bytes> [md5_hash]
 * 2. Device responds: OK READY
 * 3. Host sends: OTA DATA <base64_chunk> (multiple times)
 * 4. Device responds: OK <bytes>/<total> (<percent>%)
 * 5. Host sends: OTA END
 * 6. Device verifies MD5 (if provided), writes firmware, reboots
 */

#ifndef SERIAL_COMMAND_H
#define SERIAL_COMMAND_H

#include <Arduino.h>

// =============================================================================
// VERSION INFO
// =============================================================================

#define FIRMWARE_VERSION "1.7.2"
#define FIRMWARE_DATE    "2026-01-28"

// =============================================================================
// CONFIGURATION
// =============================================================================

#define CMD_BUFFER_SIZE     320  // Max command length (16 prefix + 240 base64 + 1 null + margin)
#define CMD_PROMPT          "> " // Command prompt (optional)
#define CAN_UPLOAD_MAX_SIZE 8192 // Max JSON config file size (8KB)

// =============================================================================
// PUBLIC API
// =============================================================================

/**
 * @brief Initialize serial command parser
 * Call once in setup() after Serial.begin()
 */
void serialCommandInit();

/**
 * @brief Process incoming serial data
 * Call in loop() to handle commands. Non-blocking.
 */
void serialCommandProcess();

/**
 * @brief Check if CAN logging is enabled
 * @return true if LOG ON was called
 */
bool isCanLogEnabled();

/**
 * @brief Check if OTA update is in progress
 * @return true if OTA START was called and not yet completed/aborted
 *
 * Use this to pause CAN processing during OTA for better throughput.
 */
bool isOtaInProgress();

#endif // SERIAL_COMMAND_H
