/**
 * @file CanCapture.cpp
 * @brief CAN frame handler using configurable processor
 *
 * This module delegates CAN frame processing to the CanConfigProcessor,
 * which uses configuration loaded from JSON files.
 * Falls back to mock data generation when no config is present.
 */

#include <ESP32-TWAI-CAN.hpp>
#include "CanConfigProcessor.h"
#include "GlobalData.h"
#include "ConfigManager.h"
#include "SerialCommand.h"
#include "CanLogFormatter.h"

#define LED_HEARTBEAT 8

// External reference to processor instance (defined in main.cpp)
extern CanConfigProcessor canProcessor;

/**
 * @brief Process a received CAN frame using the configured processor
 * @param rxFrame Reference to the received CAN frame
 * @param receivedAt Software dequeue timestamp in milliseconds
 */
void handleCanCapture(const CanFrame& rxFrame, uint32_t receivedAt) {
    // Process frame through configurable processor
    bool processed = canProcessor.processFrame(rxFrame);

    // LED heartbeat on steering frame (0x002) for activity indication
    // This is kept regardless of configuration for visual feedback
    if (rxFrame.identifier == 0x002) {
        digitalWrite(LED_HEARTBEAT, !digitalRead(LED_HEARTBEAT));
    }

    // Log CAN frame if enabled via serial command
    if (isCanLogEnabled()) {
        // Format a complete line before output; neither decoder nor logger mutates the frame.
        char line[160];
        if (formatCanFrameLog(rxFrame, receivedAt, line, sizeof(line))) Serial.println(line);
    }

    // Optional: Debug logging for unknown frames
    #ifdef DEBUG_CAN_UNKNOWN
    if (!processed) {
        Serial.printf("[CAN] Unknown frame: 0x%03X\n", rxFrame.identifier);
    }
    #endif
}
