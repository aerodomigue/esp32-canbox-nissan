#include <ESP32-TWAI-CAN.hpp>
#include "GlobalData.h"

#define LED_HEARTBEAT 8

void handleCanCapture(CanFrame &rxFrame) {
    // --- TIMER POUR LES LOGS (1 seconde) ---
    static uint32_t lastLogTime = 0;
    uint32_t now = millis();

    switch (rxFrame.identifier) {
        
        // --- 1. DIRECTION (Angle Volant) ---
        case 0x002: 
            digitalWrite(LED_HEARTBEAT, !digitalRead(LED_HEARTBEAT)); 
            // Nissan Juke : Big Endian signé. 
            // On stocke la valeur brute. L'inversion de sens se fera dans RadioSend.
            currentSteer = (int16_t)((rxFrame.data[0] << 8) | rxFrame.data[1]);
            break;

        // --- 2. MOTEUR (Régime RPM) ---
        case 0x180: 
            // Nissan Juke F15 : Octets 0 et 1. Facteur 0.25 (Diviser par 4).
            engineRPM = (uint16_t)((rxFrame.data[0] << 8) | rxFrame.data[1]) / 4;
            break;

        // --- 3. VITESSE (Roues) ---
        case 0x284: 
            // Probable octets 0 et 1 (à valider en roulant).
            // Les octets 6 et 7 étaient un compteur, on les ignore.
            vehicleSpeed = (uint16_t)(((rxFrame.data[0] << 8) | rxFrame.data[1]) / 100);
            break;

        // --- 4. COMBINÉ (Essence & Odomètre) ---
        case 0x5C5: 
            {
                // Octet 0 : Niveau Essence en % (0x44 = 68%)
                // On mappe 0-100% vers 0-45 Litres (Capacité Polo)
                uint8_t rawFuel = rxFrame.data[0];
                if (rawFuel > 100) rawFuel = 100; // Sécurité
                fuelLevel = map(rawFuel, 0, 100, 0, 45);

                // Octets 1,2,3 : Odomètre (Km Total) - Juste pour info/log
                uint32_t odo = ((uint32_t)rxFrame.data[1] << 16) | ((uint32_t)rxFrame.data[2] << 8) | rxFrame.data[3];
            }
            break;

        // --- 5. BATTERIE (Alternateur) ---
        case 0x6F6: 
            // Octet 0 : Voltage brut (0x8D = 141 -> 14.1V)
            voltBat = rxFrame.data[0] * 0.1f;
            break;

        // --- 6. TEMPERATURE (Moteur -> Extérieur) ---
        case 0x551:
            // Octet 0 : Température LDR. Formule : Valeur - 40.
            // On l'utilise pour afficher quelque chose sur le poste (faute de sonde ext.)
            tempExt = (int8_t)(rxFrame.data[0] - 40);
            break;

        // --- 7. CARROSSERIE (Portes) ---
        case 0x60D: 
            currentDoors = 0;
            // Mapping des portes Nissan vers bitmask générique
            if (rxFrame.data[0] & 0x01) currentDoors |= 0x80; // Conducteur
            if (rxFrame.data[0] & 0x02) currentDoors |= 0x40; // Passager
            if (rxFrame.data[0] & 0x04) currentDoors |= 0x20; // AR Gauche
            if (rxFrame.data[0] & 0x08) currentDoors |= 0x10; // AR Droite
            if (rxFrame.data[0] & 0x10) currentDoors |= 0x08; // Coffre
            break;

        // --- 8. INFOS DIVERSES (Conso) ---
        case 0x54C: 
            // Autonomie (DTE) souvent octets 4 et 5
            dteValue = (rxFrame.data[4] << 8) | rxFrame.data[5]; 
            break;
    }

    // --- LOGS DEBUG ---
    if (Serial && (now - lastLogTime >= 1000)) {
        Serial.println("--- NISSAN DATA DECODED ---");
        Serial.printf("RPM: %u | Speed: %u | Volt: %.1fV | Temp: %d C\n", engineRPM, vehicleSpeed, voltBat, tempExt);
        Serial.printf("Fuel: %u L (VW scale) | Steer: %d\n", fuelLevel, currentSteer);
        Serial.printf("Doors Raw: 0x%02X\n", currentDoors);
        Serial.println("---------------------------");
        lastLogTime = now;
    }
}