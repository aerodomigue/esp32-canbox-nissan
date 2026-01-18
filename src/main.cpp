#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include <esp_task_wdt.h>
#include "GlobalData.h"
#include "CanCapture.h"
#include "RadioSend.h"

// --- CONFIGURATION SECURITE ---
#define WDT_TIMEOUT 5       // Watchdog système (plantage CPU)
#define CAN_TIMEOUT 30000   // 30s sans message CAN = Reboot (si contact mis)
#define MAX_CAN_ERRORS 100  // Nombre d'erreurs max avant Reset d'urgence (Seuil CAN Passif ~127)

#define CAN_TX 21
#define CAN_RX 20

uint32_t lastCanMessageTime = 0;
HardwareSerial RadioSerial(1);

void setup() {
    // A. LED Statut
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH); // Allumée = Boot

    // B. Serial Debug
    Serial.begin(115200);
    delay(2000); 
    Serial.println("--- BOOT ESP32 (F15 Gateway) ---");

    // C. Watchdog Hardware
    esp_task_wdt_deinit();
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << 0), 
        .trigger_panic = true,
    };
    esp_task_wdt_init(&twdt_config);
    esp_task_wdt_add(NULL); 

    // D. Radio UART
    RadioSerial.begin(38400, SERIAL_8N1, 6, 5); 

    // E. Initialisation CAN
    ESP32Can.setPins(CAN_TX, CAN_RX);
    ESP32Can.setSpeed(ESP32Can.convertSpeed(TWAI_SPEED_500KBPS)); // 500kbps Juke
    
    // Essai de démarrage CAN avec gestion d'erreur immédiate
    if (!ESP32Can.begin()) {
        Serial.println("ERREUR CRITIQUE : CAN INIT FAIL -> Reboot dans 3s");
        delay(3000);
        ESP.restart();
    } else {
        Serial.println("CAN OK");
    }

    lastCanMessageTime = millis();
    digitalWrite(8, LOW); 
}

void loop() {
    CanFrame rxFrame;
    unsigned long now = millis();
    esp_task_wdt_reset(); // On nourrit le watchdog

    // ============================================================
    // 1. SECURITE : GESTION DES ERREURS CAN (AJOUT DEMANDE)
    // ============================================================
    // On vérifie les compteurs internes du contrôleur CAN (TWAI)
    uint32_t rxErr = ESP32Can.rxErrorCounter();
    uint32_t busErr = ESP32Can.busErrCounter();
    
    // Si trop d'erreurs ou si le bus s'est coupé (Bus Off)
    if (rxErr > MAX_CAN_ERRORS || busErr > MAX_CAN_ERRORS || ESP32Can.canState() == TWAI_STATE_BUS_OFF) {
        
        Serial.printf("\n!!! CRASH CAN BUS DETECTE !!!\n");
        Serial.printf("RX Err: %d | Bus Err: %d | State: %d\n", rxErr, busErr, ESP32Can.canState());
        Serial.println("-> RESET D'URGENCE DU CONTROLEUR...");
        
        // Option douce : Tenter de relancer le driver CAN sans rebooter tout l'ESP
        // ESP32Can.end();
        // delay(100);
        // ESP32Can.begin(); 
        
        // Option radicale (plus sûre lors d'un démarrage voiture) : Reboot complet
        delay(100); 
        ESP.restart();
    }

    // ============================================================
    // 2. LECTURE CAN BUS
    // ============================================================
    if (ESP32Can.readFrame(rxFrame)) {
        handleCanCapture(rxFrame);
        lastCanMessageTime = now;
        
        // Flash LED à chaque réception (Indicateur de vie)
        digitalWrite(8, !digitalRead(8)); 
    } 
    else {
        // Heartbeat lent si pas de message (Bus silencieux)
        static unsigned long lastHeartbeat = 0;
        if (now - lastCanMessageTime > 200 && now - lastHeartbeat > 1000) {
             digitalWrite(8, !digitalRead(8));
             lastHeartbeat = now;
        }
    }

    // ============================================================
    // 3. ENVOI VERS RADIO
    // ============================================================
    processRadioUpdates();

    // ============================================================
    // 4. SECURITE : TIMEOUT GLOBAL (Si moteur coupé ou fil coupé)
    // ============================================================
    // Si silence total > 30s ET que la batterie est > 11V (donc voiture non garée, juste plantée)
    if (now - lastCanMessageTime > CAN_TIMEOUT && voltBat > 11.0) {
        Serial.println("TIMEOUT SILENCE CAN -> REBOOT SECURITE");
        delay(100);
        ESP.restart(); 
    }
}