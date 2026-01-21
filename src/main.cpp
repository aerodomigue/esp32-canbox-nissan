/**
 * @file main.cpp
 * @brief ESP32 CAN Gateway for Nissan Juke F15 to VW-protocol head unit
 * 
 * This firmware acts as a bridge between the Nissan Juke F15 CAN bus and an
 * aftermarket Android head unit using VW/Polo protocol. It reads vehicle data
 * from the OBD-II CAN bus and translates it to the format expected by the radio.
 * 
 * Hardware: ESP32-S3 (or compatible) with TWAI CAN controller
 * CAN Speed: 500 kbps (Nissan standard)
 * Radio Protocol: VW Polo UART @ 38400 baud
 */

#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include <esp_task_wdt.h>
#include "GlobalData.h"
#include "CanCapture.h"
#include "RadioSend.h"

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

/**
 * @brief System initialization
 * 
 * Initializes all hardware peripherals in the following order:
 * A. Status LED
 * B. Debug Serial
 * C. Hardware Watchdog
 * D. Radio UART
 * E. CAN Controller
 */
void setup() {
    // A. Status LED - Used for boot indication and heartbeat
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH); // LED ON = Boot in progress

    // B. Debug Serial - For monitoring via USB
    Serial.begin(115200);
    delay(2000); 
    Serial.println("--- ESP32 BOOT (F15 Gateway) ---");

    // C. Hardware Watchdog - Automatic reboot on system hang
    esp_task_wdt_deinit();
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << 0), 
        .trigger_panic = true,
    };
    esp_task_wdt_init(&twdt_config);
    esp_task_wdt_add(NULL); 

    // D. Radio UART - Communication with Android head unit
    // TX=GPIO5, RX=GPIO6, 38400 baud, 8N1 (Toyota RAV4 protocol)
    RadioSerial.begin(38400, SERIAL_8N1, 6, 5); 

    // E. CAN Bus Initialization - TWAI controller setup
    ESP32Can.setPins(CAN_TX, CAN_RX);
    ESP32Can.setSpeed(ESP32Can.convertSpeed(500000)); // 500kbps (Nissan Juke standard)
    
    // Attempt CAN startup with immediate error handling
    if (!ESP32Can.begin()) {
        Serial.println("CRITICAL ERROR: CAN INIT FAILED -> Reboot in 3s");
        delay(3000);
        ESP.restart();
    } else {
        Serial.println("CAN OK");
    }

    lastCanMessageTime = millis();
    digitalWrite(8, LOW); // LED OFF = Boot complete
}

/**
 * @brief Main loop - Runs continuously after setup()
 * 
 * Execution flow:
 * 1. Feed the watchdog
 * 2. Check CAN bus health and error counters
 * 3. Read and process incoming CAN frames
 * 4. Send updates to the radio
 * 5. Monitor for communication timeout
 */
void loop() {
    CanFrame rxFrame;
    unsigned long now = millis();
    esp_task_wdt_reset(); // Feed the watchdog to prevent system reset

    // ==========================================================================
    // 1. SAFETY: CAN BUS ERROR MONITORING
    // ==========================================================================
    // Check the internal error counters of the TWAI (CAN) controller
    // These counters increment on transmission/reception errors
    uint32_t rxErr = ESP32Can.rxErrorCounter();
    uint32_t busErr = ESP32Can.busErrCounter();
    
    // Trigger emergency reset if:
    // - RX error counter exceeds threshold (approaching Error Passive state)
    // - Bus error counter exceeds threshold
    // - Controller has entered Bus Off state (disconnected from bus)

    if (Serial && (ESP32Can.rxErrorCounter() > 0 || ESP32Can.busErrCounter() > 0)) {
        Serial.printf("Erreurs RX: %d | Erreurs Bus: %d | State: %d\n", 
                      ESP32Can.rxErrorCounter(), 
                      ESP32Can.busErrCounter(),
                      ESP32Can.canState());
    }

    if (Serial) {
        Serial.printf("RX ID: 0x%03X | DLC: %d | Data: ", rxFrame.identifier, rxFrame.data_length_code);
        for (int i = 0; i < rxFrame.data_length_code; i++) {
            Serial.printf("%02X ", rxFrame.data[i]);
        }
        Serial.println();
    }
    if (rxErr > MAX_CAN_ERRORS || busErr > MAX_CAN_ERRORS || ESP32Can.canState() == TWAI_STATE_BUS_OFF) {
        
        Serial.printf("\n!!! CAN BUS CRASH DETECTED !!!\n");
        Serial.printf("RX Err: %d | Bus Err: %d | State: %d\n", rxErr, busErr, ESP32Can.canState());
        Serial.println("-> EMERGENCY CONTROLLER RESET...");
        
        // Soft option: Try to restart CAN driver without full ESP reboot
        // ESP32Can.end();
        // delay(100);
        // ESP32Can.begin(); 
        
        // Hard option (safer during vehicle startup): Full system reboot
        delay(100); 
        ESP.restart();
    }
        

    // ==========================================================================
    // 2. CAN BUS READING
    // ==========================================================================
    if (ESP32Can.readFrame(rxFrame)) {
        handleCanCapture(rxFrame);
        lastCanMessageTime = now;
        
        // Toggle LED on each received frame (activity indicator)
        digitalWrite(8, !digitalRead(8)); 
    } 
    else {
        // Slow heartbeat when no messages (indicates silent bus)
        static unsigned long lastHeartbeat = 0;
        if (now - lastCanMessageTime > 200 && now - lastHeartbeat > 1000) {
             digitalWrite(8, !digitalRead(8));
             lastHeartbeat = now;
        }
    }

    // ==========================================================================
    // 3. RADIO TRANSMISSION
    // ==========================================================================
    processRadioUpdates();

    // ==========================================================================
    // 4. SAFETY: GLOBAL TIMEOUT (Engine off or wire disconnected)
    // ==========================================================================
    // If total silence > 30s AND battery voltage > 11V (car not parked, just frozen)
    // This prevents infinite hang when CAN communication is lost but power remains
    if (now - lastCanMessageTime > CAN_TIMEOUT && voltBat > 11.0) {
        Serial.println("CAN SILENCE TIMEOUT -> SAFETY REBOOT");
        delay(100);
        ESP.restart(); 
    }
}
