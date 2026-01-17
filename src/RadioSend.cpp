#include <Arduino.h>
#include "GlobalData.h"

extern HardwareSerial RadioSerial;

// Helper mapping pour l'angle du volant
long map_float(float x, float in_min, float in_max, float out_min, float out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

void transmettreVersPoste(uint8_t commande, uint8_t* donnees, uint8_t longueur) {
    uint8_t sum = commande + longueur;
    for (int i = 0; i < longueur; i++) { sum += donnees[i]; }
    uint8_t checksum = sum ^ 0xFF;

    RadioSerial.write(0x2E);
    RadioSerial.write(commande);
    RadioSerial.write(longueur);
    RadioSerial.write(donnees, longueur);
    RadioSerial.write(checksum);
}

void handshake() {
    while (RadioSerial.available() > 0) {
        uint8_t head = RadioSerial.read();
        if (head == 0x2E) {
            delay(5);
            uint8_t cmd = RadioSerial.read();
            uint8_t len = RadioSerial.read();
            for (int i = 0; i < len + 1; i++) { RadioSerial.read(); }
            
            if (cmd == 0xC0 || cmd == 0x08) {
                uint8_t ver[] = {0x02, 0x08, 0x10}; 
                transmettreVersPoste(0xF1, ver, 3); 
            }
            if (cmd == 0x90) {
                uint8_t ok[] = {0x41, 0x02}; 
                transmettreVersPoste(0x91, ok, 2); 
            }
        }
    }
}

void processRadioUpdates() {
    static unsigned long lastFastTime = 0;
    static unsigned long lastSlowTime = 0;
    unsigned long now = millis();

    handshake();

    // --- 1. VOLANT (ID 0x26) ---
    if (now - lastFastTime >= 100) {
        int16_t angleVW = (int16_t)map(currentSteer, -500, 500, -5400, 5400);
        uint8_t payloadSteer[2] = { (uint8_t)(angleVW & 0xFF), (uint8_t)(angleVW >> 8) };
        transmettreVersPoste(0x26, payloadSteer, 2); 
        lastFastTime = now;
    }

    // --- 2. DASHBOARD (ID 0x41, Sub 0x02) ---
    if (now - lastSlowTime >= 400) {
        int16_t tRaw = (int16_t)(tempExt * 10.0f);
        uint16_t vBatScaled = (uint16_t)(voltBat * 100.0f);

        uint8_t payload41[13] = {0};
        payload41[0]  = 0x02; // Sous-commande Dashboard
        payload41[1]  = (uint8_t)(engineRPM >> 8);
        payload41[2]  = (uint8_t)(engineRPM & 0xFF);
        payload41[5]  = (uint8_t)(vBatScaled >> 8);
        payload41[6]  = (uint8_t)(vBatScaled & 0xFF);
        payload41[7]  = (uint8_t)(tRaw >> 8);
        payload41[8]  = (uint8_t)(tRaw & 0xFF);
        payload41[11] = 0x04; // Status moteur
        payload41[12] = (uint8_t)fuelLevel;

        transmettreVersPoste(0x41, payload41, 13);
        lastSlowTime = now;
    }

    // --- 3. PORTES UNIQUEMENT (ID 0x41, Sub 0x01) ---
    // static uint8_t lastSentDoors = 0xFF;
    
    // // On compare avec un masque pour ignorer les changements du bit 0 (Frein à main)
    // if ((currentDoors & 0xFE) != (lastSentDoors & 0xFE)) {
        
    //     uint8_t payloadDoors[13] = {0};
    //     payloadDoors[0] = 0x01;         // Sous-commande Portes
        
    //     // On envoie currentDoors MAIS on force le bit 0 à 0 (Masque 0xFE)
    //     // Nissan 0x60D : 0x80, 0x40, 0x20, 0x10, 0x08 (Portes/Coffre)
    //     // Le bit 0x01 (Frein) est ainsi supprimé du signal envoyé au poste.
    //     payloadDoors[1] = currentDoors & 0xFE; 
        
    //     transmettreVersPoste(0x41, payloadDoors, 13);
        
    //     // On met à jour l'état précédent (en gardant le bit 0 pour la comparaison future)
    //     lastSentDoors = currentDoors;
    // }

    // --- 3. TEST COFFRE (ID 0x41, Sub 0x01) ---
    static uint8_t lastSentDoors = 0xFF;
    if (currentDoors != lastSentDoors) {
        uint8_t payload[13] = {0};
        payload[0] = 0x01; 

        uint8_t vw = 0;
        // On mappe tes cases vers les bits standards VW Polo
        if (currentDoors & 0x80) vw |= 0x01; // Case 1: AVG (On remet 0x01 !)
        if (currentDoors & 0x40) vw |= 0x02; // Case 2: AVD
        if (currentDoors & 0x20) vw |= 0x04; // Case 3: ARG
        if (currentDoors & 0x10) vw |= 0x08; // Case 4: ARD
        if (currentDoors & 0x08) vw |= 0x10; // Case 5: Coffre

        payload[1] = vw; 
        transmettreVersPoste(0x41, payload, 13);
        lastSentDoors = currentDoors;
    } 
}