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
 */

#ifndef SERIAL_COMMAND_H
#define SERIAL_COMMAND_H

#include <Arduino.h>

// =============================================================================
// VERSION INFO
// =============================================================================

// APP_VERSION is defined as a compiler flag (-DAPP_VERSION="v1.x.x")
// If not defined, fallback to "dev"
#ifndef APP_VERSION
  #define APP_VERSION "dev"
#endif

#define FIRMWARE_VERSION APP_VERSION
#define FIRMWARE_DATE    __DATE__

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
 */
void serialCommandInit();

/**
 * @brief Process incoming serial data
 */
void serialCommandProcess();

/**
 * @brief Check if CAN logging is enabled
 * @return true if LOG ON was called
 */
bool isCanLogEnabled();

/**
 * @brief Check if OTA update is in progress
 */
bool isOtaInProgress();

#endif // SERIAL_COMMAND_H
