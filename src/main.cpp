#include <Arduino.h>
#include <esp_task_wdt.h> // Pour le Watchdog
#include "GlobalData.h"
#include "CanCapture.h"
#include "RadioSend.h"

#define WDT_TIMEOUT 5      // 5 secondes sans signe de vie = Reboot
#define CAN_TIMEOUT 30000  // 30 secondes sans trames CAN = Reboot de sécurité
uint32_t lastCanMessageTime = 0;

HardwareSerial &RadioSerial = Serial1;
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
    Serial1.begin(38400, SERIAL_8N1, 6, 5); 
    Serial.println("Radio UART OK");

    // E. Initialisation CAN (Sur GPIO 20 et 21)
    Serial.println("Init CAN sur 20/21...");
    
    // Si l'initialisation échoue (problème hardware interne/pins)
    if (!ESP32Can.begin(ESP32Can.convertSpeed(500), 20, 21)) {
        Serial.println("ERREUR CRITIQUE : CAN HARDWARE FAIL");
        
        // --- ETAT LED : STROBOSCOPE (50ms) ---
        // Signifie : Erreur d'initialisation CAN (Pins ou Puce HS)
        while(1) {
            digitalWrite(8, !digitalRead(8));
            delay(50); 
            esp_task_wdt_reset(); // On nourrit le chien pour ne pas rebooter
        }
    }
    Serial.println("CAN OK ! Pret a recevoir.");

    lastCanMessageTime = millis();
    digitalWrite(8, LOW); // On éteint avant de commencer la boucle
}

void loop() {
    // On réinitialise le Watchdog à chaque tour de boucle
    esp_task_wdt_reset();

    // --- PARTIE CAPTURE ---
    CanFrame rxFrame;
    if (ESP32Can.readFrame(rxFrame)) {
        handleCanCapture(rxFrame);
        lastCanMessageTime = millis(); // On a reçu quelque chose !
    }

    // --- PARTIE ÉMISSION ---
    processRadioUpdates();

    // --- SÉCURITÉ : WATCHDOG CAN ---
    // Si on n'a rien reçu du bus CAN depuis 30s alors que le contact est mis
    // (on vérifie la tension batterie pour ne pas rebooter quand la voiture est garée)
    if (millis() - lastCanMessageTime > CAN_TIMEOUT && voltBat > 11.0) {
        ESP.restart(); 
    }
}