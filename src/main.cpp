/**
 * @file main.cpp
 * @brief ESP32 CAN Gateway for Nissan Juke F15 to Android head unit
 *
 * This firmware acts as a bridge between the Nissan Juke F15 CAN bus and an
 * aftermarket Android head unit using Toyota RAV4 protocol. It reads vehicle data
 * from the OBD-II CAN bus and translates it to the format expected by the radio.
 *
 * Hardware: ESP32-C3 (or compatible) with TWAI CAN controller
 * CAN Speed: 500 kbps (Nissan standard)
 * Radio Protocol: Toyota RAV4 UART @ 38400 baud (Analysis by Mikescotland)
 */

#include "UsbConsole.h"
#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include <esp_task_wdt.h>
#include <freertos/task.h>
#include "GlobalData.h"
#include "ConfigManager.h"
#include "SerialCommand.h"
#include "CanCapture.h"
#include "RadioSend.h"
#include "CanConfigProcessor.h"
#include "MockDataGenerator.h"
#include "ObdCalibration.h"

// ==============================================================================
// SAFETY CONFIGURATION
// ==============================================================================
#define WDT_TIMEOUT 5       // Hardware watchdog timeout in seconds (triggers panic on CPU hang)
#define CAN_TIMEOUT 30000   // 30s without CAN messages triggers reboot (if ignition is on)
#define MAX_CAN_ERRORS 100  // Max error count before emergency reset (CAN passive threshold ~127)

// ==============================================================================
// HARDWARE PIN CONFIGURATION
// ==============================================================================
#define CAN_TX 21
#define CAN_RX 20

// ==============================================================================
// GLOBAL VARIABLES
// ==============================================================================
uint32_t lastCanMessageTime = 0;
HardwareSerial RadioSerial(1);

// Configurable CAN processor and mock data generator
CanConfigProcessor canProcessor;
MockDataGenerator mockGenerator;

static bool sendObdReference(const CanFrame& frame, void*) {
    return ESP32Can.writeFrame(frame, 0);  // Queued, not a bus acknowledgement.
}

static void printObdReference(const char* line, void*) {
    if (!isOtaInProgress()) Console.logLine(line, true);
}

ObdCalibration obdCalibration(sendObdReference, printObdReference);

/**
 * @brief System initialization
 *
 * Initializes all hardware peripherals in the following order:
 * A. Status LED
 * B. Debug Serial
 * C. Configuration (NVS)
 * D. Serial Command Interface
 * E. Hardware Watchdog
 * F. Radio UART
 * G. CAN Configuration (JSON or Mock)
 * H. CAN Controller (if real mode)
 */
void setup() {
    // A. Status LED - Used for boot indication and heartbeat
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH); // LED ON = Boot in progress

    // B. Debug Serial - For monitoring via USB
    Console.begin();
    delay(2000);
    Console.beginCommand();
    Console.println("--- ESP32 BOOT (F15 Gateway) ---");

    // C. Load configuration from NVS (or use defaults on first boot)
    configInit();
    Console.println("Config loaded");

    // D. Initialize serial command interface
    serialCommandInit();

    // E. Hardware Watchdog - Automatic reboot on system hang
    esp_task_wdt_deinit();
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << 0),
        .trigger_panic = true,
    };
    esp_task_wdt_init(&twdt_config);
    esp_task_wdt_add(NULL);

    // F. Radio UART - Communication with Android head unit
    // TX=GPIO5, RX=GPIO6, 38400 baud, 8N1 (Toyota RAV4 protocol)
    RadioSerial.begin(38400, SERIAL_8N1, 6, 5);

    // G. CAN Configuration - Load from JSON or use mock mode
    canProcessor.begin();  // Attempts to load /vehicle.json or /NissanJukeF15.json

    if (canProcessor.isMockMode()) {
        Console.println("=== MOCK MODE ACTIVE ===");
        Console.println("No vehicle config found - using simulated data");
        mockGenerator.begin();
        // Skip CAN hardware initialization in mock mode
    } else {
        Console.printf("Vehicle config loaded: %s\n", canProcessor.getProfileName());

        // H. CAN Bus Initialization - TWAI controller setup (only in real mode)
        ESP32Can.setPins(CAN_TX, CAN_RX);
        ESP32Can.setSpeed(ESP32Can.convertSpeed(500)); // API uses kbps (Nissan Juke: 500 kbps)
        ESP32Can.setRxQueueSize(128);

        // Attempt CAN startup with immediate error handling
        if (!ESP32Can.begin()) {
            Console.println("CRITICAL ERROR: CAN INIT FAILED -> Reboot in 3s");
            Console.drainBeforeRestart(3000);
            ESP.restart();
        } else {
            Console.println("CAN OK");
        }
    }

    lastCanMessageTime = millis();
    digitalWrite(8, LOW); // LED OFF = Boot complete
    Console.endCommand();
    Console.drain();
}

/**
 * @brief Main loop - Runs continuously after setup()
 *
 * Execution flow:
 * 1. Feed the watchdog
 * 2. Process serial commands
 * 3. Mode-dependent data acquisition (mock or real CAN)
 * 4. Send updates to the radio
 */
void loop() {
    unsigned long now = millis();
    esp_task_wdt_reset(); // Feed the watchdog to prevent system reset

    Console.drain();

    // Process serial commands (non-blocking)
    canProcessor.updateFuelConsumption(now);
    serialCommandProcess();

    // ==========================================================================
    // MODE-DEPENDENT DATA ACQUISITION
    // Skip during OTA for maximum throughput
    // ==========================================================================
    if (isOtaInProgress()) {
        obdCalibration.setEnabled(false, millis());
        serialCommandCheckOtaTimeout();
        Console.drain();
        vTaskDelay(pdMS_TO_TICKS(1));  // yield to USB CDC FreeRTOS tasks
        return;
    }

    if (canProcessor.isMockMode()) {
        obdCalibration.setEnabled(false, millis());
        // MOCK MODE: Generate simulated data
        mockGenerator.update();
        lastCanMessageTime = now;  // Prevent timeout in mock mode

        // Slow LED blink to indicate mock mode
        static unsigned long lastMockBlink = 0;
        if (now - lastMockBlink > 500) {
            digitalWrite(8, !digitalRead(8));
            lastMockBlink = now;
        }
    } else {
        obdCalibration.setEnabled(isCanLogEnabled(), millis());
        // REAL MODE: Read from CAN bus
        CanFrame rxFrame = {};

        // CAN BUS ERROR MONITORING
        uint32_t rxErr = ESP32Can.rxErrorCounter();
        uint32_t busErr = ESP32Can.busErrCounter();

        if (rxErr > MAX_CAN_ERRORS || busErr > MAX_CAN_ERRORS || ESP32Can.canState() == TWAI_STATE_BUS_OFF) {
            Console.printf("\n!!! CAN BUS CRASH DETECTED !!!\n");
            Console.printf("RX Err: %d | Bus Err: %d | State: %d\n", rxErr, busErr, ESP32Can.canState());
            Console.println("-> EMERGENCY CONTROLLER RESET...");
            Console.drainBeforeRestart(100);
            ESP.restart();
        }

        // CAN BUS READING
        bool received = false;
        for (uint8_t n = 0; n < 32 && ESP32Can.readFrame(rxFrame, 0); ++n) {
            const uint32_t receivedAt = millis();
            received = true;
            handleCanCapture(rxFrame, receivedAt);
            obdCalibration.receive(rxFrame, receivedAt);
            lastCanMessageTime = receivedAt;
        }
        obdCalibration.tick(millis());
        // Expose actual driver losses; an enlarged queue does not guarantee a lossless capture.
        static uint32_t lastStats = 0;
        if (isCanLogEnabled() && uint32_t(millis() - lastStats) >= 1000) {
            lastStats = millis();
            char statistics[256];
            snprintf(statistics, sizeof(statistics), "ODB DRIVER t_ms=%lu rx_missed=%lu tx_failed=%lu rx_queue=%lu rx_errors=%lu bus_errors=%lu state=%d",
                          static_cast<unsigned long>(lastStats),
                          static_cast<unsigned long>(ESP32Can.rxMissedCounter()),
                          static_cast<unsigned long>(ESP32Can.txFailedCounter()),
                          static_cast<unsigned long>(ESP32Can.inRxQueue()),
                          static_cast<unsigned long>(rxErr),
                          static_cast<unsigned long>(busErr),
                          static_cast<int>(ESP32Can.canState()));
            Console.logLine(statistics, true);
            snprintf(statistics, sizeof(statistics),
                     "ODB BUFFER t_ms=%lu queued_bytes=%u dropped_raw=%lu dropped_odb=%lu short_writes=%lu rejected_responses=%lu",
                     static_cast<unsigned long>(lastStats),
                     static_cast<unsigned>(Console.queuedBytes()),
                     static_cast<unsigned long>(Console.droppedRawLines()),
                     static_cast<unsigned long>(Console.droppedDiagnosticLines()),
                     static_cast<unsigned long>(Console.shortWrites()),
                     static_cast<unsigned long>(Console.rejectedResponses()));
            Console.logLine(statistics, true);
        }
        if (!received) {
            // Slow heartbeat when no messages (indicates silent bus)
            static unsigned long lastHeartbeat = 0;
            if (now - lastCanMessageTime > 200 && now - lastHeartbeat > 1000) {
                digitalWrite(8, !digitalRead(8));
                lastHeartbeat = now;
            }
        }

        // SAFETY: GLOBAL TIMEOUT (Engine off or wire disconnected)
        if (uint32_t(millis() - lastCanMessageTime) > CAN_TIMEOUT && voltBat > 11.0) {
            Console.println("CAN SILENCE TIMEOUT -> SAFETY REBOOT");
            Console.drainBeforeRestart(100);
            ESP.restart();
        }
    }

    // ==========================================================================
    // RADIO TRANSMISSION (both modes)
    // ==========================================================================
    canProcessor.updateFuelConsumption(millis());
    processRadioUpdates();
    Console.drain();
}
