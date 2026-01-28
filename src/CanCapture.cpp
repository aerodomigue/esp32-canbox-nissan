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

#define LED_HEARTBEAT 8

// External reference to processor instance (defined in main.cpp)
extern CanConfigProcessor canProcessor;

/**
 * @brief Process a received CAN frame using the configured processor
 * @param rxFrame Reference to the received CAN frame
 */
void handleCanCapture(CanFrame &rxFrame) {
    // Process frame through configurable processor
    bool processed = canProcessor.processFrame(rxFrame);

    // LED heartbeat on steering frame (0x002) for activity indication
    // This is kept regardless of configuration for visual feedback
    if (rxFrame.identifier == 0x002) {
        digitalWrite(LED_HEARTBEAT, !digitalRead(LED_HEARTBEAT));
    }

    // Log CAN frame if enabled via serial command
    if (isCanLogEnabled()) {
        Serial.printf("RX 0x%03X [%d]: ", rxFrame.identifier, rxFrame.data_length_code);
        for (int i = 0; i < rxFrame.data_length_code; i++) {
            Serial.printf("%02X ", rxFrame.data[i]);
        }
        Serial.println();
    }

    // Optional: Debug logging for unknown frames
    #ifdef DEBUG_CAN_UNKNOWN
    if (!processed) {
        Serial.printf("[CAN] Unknown frame: 0x%03X\n", rxFrame.identifier);
    }
    #endif
}
