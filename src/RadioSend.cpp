#include <Arduino.h>
#include "GlobalData.h"

extern HardwareSerial RadioSerial;

// --- AJOUT POUR LES LOGS ---
static uint32_t lastLogRadio = 0;

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
    // Nissan Juke F15 : 0.1 degré par unité (comme VW)
    // On inverse juste le signe (-) car Nissan et VW tournent souvent en sens opposé
    // Si vos lignes tournent à l'envers sur l'écran, enlevez le signe moins.
    int16_t angleVW = -currentSteer; 

    if (now - lastFastTime >= 100) {
        // Envoi en Little Endian (Standard VW)
        uint8_t payloadSteer[2] = { (uint8_t)(angleVW & 0xFF), (uint8_t)(angleVW >> 8) };
        transmettreVersPoste(0x26, payloadSteer, 2); 
        lastFastTime = now;
    }

// --- 2. DASHBOARD (Vitesse, RPM, Handbrake) - ID 0x41 Sub 0x02 ---
    if (now - lastSlowTime >= 400) {
        int16_t tRaw = (int16_t)(tempExt * 10.0f);
        uint16_t vBatScaled = (uint16_t)(voltBat * 100.0f);
        
        // Conversion Vitesse : km/h -> échelle VW (x100)
        uint16_t speedScaled = vehicleSpeed * 100; 

        uint8_t payload41[13] = {0};
        payload41[0]  = 0x02; // Sous-commande Dashboard
        
        // RPM (Octets 1-2)
        payload41[1]  = (uint8_t)(engineRPM >> 8);
        payload41[2]  = (uint8_t)(engineRPM & 0xFF);

        // VITESSE (Octets 3-4)
        payload41[3]  = (uint8_t)(speedScaled & 0xFF); // LSB
        payload41[4]  = (uint8_t)(speedScaled >> 8);   // MSB

        // Batterie (Octets 5-6)
        payload41[5]  = (uint8_t)(vBatScaled >> 8);
        payload41[6]  = (uint8_t)(vBatScaled & 0xFF);

        // Température (Octets 7-8)
        payload41[7]  = (uint8_t)(tRaw >> 8);
        payload41[8]  = (uint8_t)(tRaw & 0xFF);

        // Ceinture (Bit 1) et autres statuts
        // On peut mettre 0x04 pour dire "Moteur démarré" ou "Contact mis"
        uint8_t statusByte = 0x04; 

        // FREIN A MAIN (Bit 0 de l'octet 11)
        // On récupère le bit stocké dans currentDoors bit 0 (voir CanCapture)
        if (currentDoors & 0x01) {
             statusByte |= 0x01; // Active le bit frein à main
        }
        payload41[11] = statusByte; 

        // Essence (Octet 12)
        payload41[12] = (uint8_t)fuelLevel;

        transmettreVersPoste(0x41, payload41, 13);
        lastSlowTime = now;
    }

    // --- 3. PORTES UNIQUEMENT (ID 0x41, Sub 0x01) ---
    static uint8_t lastSentDoors = 0xFF;
    uint8_t vw = 0;
    
    // Mapping porte, bits VW standards pour Polo
    if (currentDoors & 0x80) vw |= 0x01; // Nissan Cond -> VW AVG
    if (currentDoors & 0x40) vw |= 0x02; // Nissan Pass -> VW AVD
    if (currentDoors & 0x20) vw |= 0x04; // Nissan AG   -> VW ARG
    if (currentDoors & 0x10) vw |= 0x08; // Nissan AD   -> VW ARD
    if (currentDoors & 0x08) vw |= 0x10; // Nissan Coffre -> VW Coffre

    if (currentDoors != lastSentDoors) {
        uint8_t payload[13] = {0};
        payload[0] = 0x01; 
        payload[1] = vw; // Le bit 0x01 (frein à main) est ignoré ici par le mapping
        transmettreVersPoste(0x41, payload, 13);
        lastSentDoors = currentDoors;
    }

    // --- 4. LOGS D'ENVOI (Toutes les secondes si Serial connecté) ---
    if (Serial && (now - lastLogRadio >= 1000)) {
        Serial.println(">>> RADIO TX STATUS >>>");
        Serial.printf("RPM: %u | Fuel: %u%%\n", engineRPM, fuelLevel);
        Serial.printf("VW Steer: %d | VW Doors: 0x%02X\n", angleVW, vw);
        Serial.printf("VW Bat Raw: %u | VW Temp Raw: %d\n", (uint16_t)(voltBat * 100), (int16_t)(tempExt * 10));
        Serial.println("-----------------------");
        lastLogRadio = now;
    }
}