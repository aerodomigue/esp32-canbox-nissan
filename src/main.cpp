#include <Arduino.h>
#include <esp_task_wdt.h> // Pour le Watchdog
#include "GlobalData.h"
#include "CanCapture.h"
#include "RadioSend.h"

#define WDT_TIMEOUT 5      // 5 secondes sans signe de vie = Reboot
#define CAN_TIMEOUT 30000  // 30 secondes sans trames CAN = Reboot de sécurité
uint32_t lastCanMessageTime = 0;

void setup() {
    // 1. Initialisation de la LED de statut
    pinMode(8, OUTPUT);
    digitalWrite(8, HIGH); // Allumée fixe pendant l'init

    // 2. Initialisation du Watchdog Hardware
    esp_task_wdt_init(WDT_TIMEOUT, true);
    esp_task_wdt_add(NULL);

    // 3. Initialisation des communications
    Serial.begin(115200); 
    // UART1 pour la radio (TX sur GPIO 1)
    Serial1.begin(38400, SERIAL_8N1, -1, 1); 

    // 4. Initialisation du bus CAN (TWAI)
    // GPIO 20 (RX), GPIO 21 (TX)
    if (!ESP32Can.begin(ESP32Can.convertSpeed(500), 20, 21)) {
        // CODE ERREUR MORSE : Clignotement très rapide = Panne Hardware CAN
        while(1) {
            digitalWrite(8, !digitalRead(8));
            delay(50); 
            // On ne reset pas le Watchdog ici pour forcer un reboot
        }
    }

    lastCanMessageTime = millis();
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