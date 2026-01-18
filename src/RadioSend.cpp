#include <Arduino.h>
#include "GlobalData.h"

extern HardwareSerial RadioSerial;

// --- AJOUT POUR LES LOGS ---
static uint32_t lastLogRadio = 0;

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
    int16_t angleVW = (int16_t)map(currentSteer, -500, 500, -5400, 5400);
    if (now - lastFastTime >= 100) {
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
        
        // RPM (Octets 1 et 2)
        payload41[1]  = (uint8_t)(engineRPM >> 8);
        payload41[2]  = (uint8_t)(engineRPM & 0xFF);

        // Tension Batterie (Octets 5 et 6)
        payload41[5]  = (uint8_t)(vBatScaled >> 8);
        payload41[6]  = (uint8_t)(vBatScaled & 0xFF);

        // Température (Octets 7 et 8)
        payload41[7]  = (uint8_t)(tRaw >> 8);
        payload41[8]  = (uint8_t)(tRaw & 0xFF);

        // Status Moteur (0x04 = moteur tournant / contact mis)
        payload41[11] = 0x04; // Status moteur

        // ESSENCE (Octet 12) - VW attend une valeur de 0 à 100
        payload41[12] = (uint8_t)fuelLevel;

        transmettreVersPoste(0x41, payload41, 13);
        lastSlowTime = now;
    }

    // --- 3. PORTES UNIQUEMENT (ID 0x41, Sub 0x01) ---
    static uint8_t lastSentDoors = 0xFF;
    uint8_t vw = 0;
    
    // On calcule le mapping VW systématiquement pour le log, mais on n'envoie que si ça change
    if (currentDoors & 0x80) vw |= 0x01; // AVG
    if (currentDoors & 0x40) vw |= 0x02; // AVD
    if (currentDoors & 0x20) vw |= 0x04; // ARG
    if (currentDoors & 0x10) vw |= 0x08; // ARD
    if (currentDoors & 0x08) vw |= 0x10; // Coffre

    if (currentDoors != lastSentDoors) {
        uint8_t payload[13] = {0};
        payload[0] = 0x01; 
        payload[1] = vw; 
        transmettreVersPoste(0x41, payload, 13);
        lastSentDoors = currentDoors;
    }

    // --- 4. LOGS D'ENVOI (Toutes les secondes) ---
    if (Serial && (now - lastLogRadio >= 1000)) {
        Serial.println(">>> RADIO TX STATUS >>>");
        Serial.printf("RPM: %u | Fuel: %u%%\n", engineRPM, fuelLevel);
        Serial.printf("VW Steer: %d | VW Doors: 0x%02X\n", angleVW, vw);
        Serial.printf("VW Bat: %u | VW Temp: %d\n", (uint16_t)(voltBat * 100), (int16_t)(tempExt * 10));
        Serial.println("-----------------------");
        lastLogRadio = now;
    }
}