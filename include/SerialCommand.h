/**
 * @file SerialCommand.h
 * @brief Serial command parser for USB configuration interface
 *
 * Provides a simple AT-style command interface for:
 * - Reading/writing configuration parameters (CFG)
 * - Enabling/disabling CAN debug logs (LOG)
 * - System information and control (SYS)
 *
 * Commands are newline-terminated and case-insensitive.
 */

#ifndef SERIAL_COMMAND_H
#define SERIAL_COMMAND_H

#include <Arduino.h>

// =============================================================================
// VERSION INFO
// =============================================================================

#define FIRMWARE_VERSION "1.5.0"
#define FIRMWARE_DATE    "2026-01-22"

// =============================================================================
// CONFIGURATION
// =============================================================================

#define CMD_BUFFER_SIZE 64   // Max command length
#define CMD_PROMPT      "> " // Command prompt (optional)

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

#endif // SERIAL_COMMAND_H
