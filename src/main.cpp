#include <Arduino.h>
#include <ESP32-TWAI-CAN.hpp>
#include <esp_task_wdt.h> // Pour le Watchdog
#include "GlobalData.h"
#include "CanCapture.h"
#include "RadioSend.h"

#define WDT_TIMEOUT 5      // 5 secondes sans signe de vie = Reboot
#define CAN_TIMEOUT 30000  // 30 secondes sans trames CAN = Reboot de sécurité
#define CAN_TX 21
#define CAN_RX 20
uint32_t lastCanMessageTime = 0;

HardwareSerial RadioSerial(1);

void setup() {
    // A. LED Statut
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH); // Allumée = Boot en cours

    // B. Initialisation Serial (Debug via USB natif)
    Serial.begin(115200);
    delay(2000); 
    Serial.println("--- BOOT ESP32 ---");

    // C. Initialisation Watchdog (Correctif v3.0)
    esp_task_wdt_deinit(); 
    
    esp_task_wdt_config_t twdt_config = {
        .timeout_ms = WDT_TIMEOUT * 1000,
        .idle_core_mask = (1 << 0), 
        .trigger_panic = true,
    };
    if (esp_task_wdt_init(&twdt_config) != ESP_OK) {
        Serial.println("Erreur WDT Init");
    }
    esp_task_wdt_add(NULL); 
    Serial.println("Watchdog OK");

    // D. Initialisation Radio (UART1 sur GPIO 6 et 5)
    RadioSerial.begin(38400, SERIAL_8N1, 6, 5); 
    Serial.println("Radio UART OK");

    // E. Initialisation CAN (Sur GPIO 20 et 21)
    Serial.println("Init CAN ...");
    
    // Si l'initialisation échoue (problème hardware interne/pins)
    // Configuration des pins natives de l'ESP32-C3/S3
    ESP32Can.setPins(CAN_TX, CAN_RX);
    // Vitesse 500kbps pour Nissan Juke
    ESP32Can.setSpeed(ESP32Can.convertSpeed(500000));
    if (!ESP32Can.begin()) {
        Serial.println("ERREUR CRITIQUE : CAN HARDWARE FAIL");
        
        // --- ETAT LED : STROBOSCOPE (50ms) ---
        // Signifie : Erreur d'initialisation CAN (Pins ou Puce HS)
        while(1) {
            digitalWrite(8, !digitalRead(8));
            delay(50); 
            esp_task_wdt_reset(); // On nourrit le chien pour ne pas rebooter
        }
    } else {
        Serial.println("CAN OK ! Pret a recevoir.");
    }

    lastCanMessageTime = millis();
    digitalWrite(8, LOW); // On éteint avant de commencer la boucle
}

void loop() {

    CanFrame rxFrame;
    unsigned long now = millis();
    esp_task_wdt_reset();

    if (Serial && (ESP32Can.rxErrorCounter() > 0 || ESP32Can.busErrCounter() > 0)) {
        Serial.printf("Erreurs RX: %d | Erreurs Bus: %d | State: %d\n", 
                      ESP32Can.rxErrorCounter(), 
                      ESP32Can.busErrCounter(),
                      ESP32Can.canState());
    }

// // =========================================================================
//     // --- PARTIE FAKE DATA COMPLETE (SIMULATION TOTALE) ---
//     // =========================================================================
//     static unsigned long lastFakeUpdate = 0;
//     static unsigned long lastDoorChange = 0;
//     static uint8_t doorTestStep = 0;
    
//     // Variables de direction pour l'animation (Ping-Pong)
//     static int dirSteer = 1;
//     static int dirRpm = 1;
//     static int dirSpeed = 1;

//     // Mise à jour rapide (50ms) pour fluidité des aiguilles
//     if (now - lastFakeUpdate > 50) {

//         // 1. VOLANT (Simulation valeurs brutes Nissan : -7000 à +7000)
//         // Cela permet de tester si ton map_float dans RadioSend fonctionne bien
//         currentSteer = 6000;

//         // 2. RPM (800 à 6000 tr/min)
//         engineRPM = 5500;

//         // 3. VITESSE (0 à 180 km/h) - On la fait varier doucement
//         // On utilise un compteur séparé pour ne pas qu'elle bouge trop vite
//         static uint8_t speedCounter = 130;

//         // 4. AUTRES DONNÉES (Valeurs fixes ou lentes pour validation)
//         fuelLevel = 75;         // 75% (Jauge fixe pour vérifier l'affichage)
//         voltBat = 14.2;         // 14.2V (Alternateur OK)
//         tempExt = 19;           // 19°C
//         dteValue = 420;         // 420 km d'autonomie restante
//         fuelConsoMoy = 6.5;     // 6.5 L/100km

//         // 5. PORTES (Cycle séquentiel toutes les 500ms)
//         if (now - lastDoorChange > 500) {
//             doorTestStep++;
//             if (doorTestStep > 5) doorTestStep = 0;

//             switch (doorTestStep) {
//                 case 0: currentDoors = 0x00; break;
//                 case 1: currentDoors = 0x80; break; // Nissan Conducteur
//                 case 2: currentDoors = 0x40; break; // Nissan Passager
//                 case 3: currentDoors = 0x20; break; // Nissan AR Gauche
//                 case 4: currentDoors = 0x10; break; // Nissan AR Droite
//                 case 5: currentDoors = 0x08; break; // Nissan Coffre
//             }
//             lastDoorChange = now;
//         }
//         // currentDoors = 0x81;
        
//         lastFakeUpdate = now;
//     }
    // =========================================================================

    // --- PARTIE CAPTURE ---
    if (ESP32Can.readFrame(rxFrame)) {
        handleCanCapture(rxFrame);
        lastCanMessageTime = now; // Mise à jour du dernier message reçu

        // if (Serial) {
        //      Serial.printf("RX ID: 0x%03X | Data: %02X\n", rxFrame.identifier, rxFrame.data[0]);
        // }
        
        // --- ETAT LED : FLASHS NERVEUX ---
        // Signifie : Données reçues (Tout va bien)
        // On inverse l'état de la LED à chaque trame -> Scintillement
        digitalWrite(8, !digitalRead(8));
    } 
    else {
        // --- GESTION DU SILENCE ---
        // Si on n'a rien reçu depuis plus de 200ms...
        if (now - lastCanMessageTime > 200) {
             
             // --- ETAT LED : BATTEMENT DE COEUR (1s) ---
             // Signifie : "Je cherche..." (Bus silencieux ou Fils inversés)
             static unsigned long lastHeartbeat = 0;
             if (now - lastHeartbeat > 1000) {
                 digitalWrite(8, !digitalRead(8));
                 lastHeartbeat = now;
             }
        }
    }

    // --- PARTIE ÉMISSION ---
    processRadioUpdates();

    // --- SÉCURITÉ : WATCHDOG CAN ---
    // Si on n'a rien reçu du bus CAN depuis 30s alors que le contact est mis
    // (on vérifie la tension batterie > 11V pour ne pas rebooter quand la voiture est garée)
    if (now - lastCanMessageTime > CAN_TIMEOUT && voltBat > 11.0) {
        // Petit message de debug avant reboot
        Serial.println("TIMEOUT CAN -> REBOOT SECURITE");
        delay(100);
        ESP.restart(); 
    }
}