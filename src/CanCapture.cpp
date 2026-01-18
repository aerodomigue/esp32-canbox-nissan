#include <ESP32-TWAI-CAN.hpp>
#include "GlobalData.h"

#define LED_HEARTBEAT 8

void handleCanCapture(CanFrame &rxFrame) {
    // --- TIMER POUR LES LOGS (1 seconde) ---
    static uint32_t lastLogTime = 0;
    uint32_t now = millis();

    switch (rxFrame.identifier) {
        
        case 0x002: // --- DIRECTION (Angle Volant) ---
            digitalWrite(LED_HEARTBEAT, !digitalRead(LED_HEARTBEAT)); 
            currentSteer = (int16_t)((rxFrame.data[0] << 8) | rxFrame.data[1]);
            break;

        case 0x156: // --- MOTEUR (Régime RPM) ---
            engineRPM = ((rxFrame.data[1] << 8) | rxFrame.data[2]) / 8;
            break;

        case 0x284: // --- ROUES (Vitesse véhicule) ---
            vehicleSpeed = ((rxFrame.data[0] << 8) | rxFrame.data[1]) / 100;
            break;

        case 0x60D: // --- CARROSSERIE (BCM) ---
            currentDoors = 0;
            if (rxFrame.data[0] & 0x01) currentDoors |= 0x80; // Conducteur
            if (rxFrame.data[0] & 0x02) currentDoors |= 0x40; // Passager
            if (rxFrame.data[0] & 0x04) currentDoors |= 0x20; // AR Gauche
            if (rxFrame.data[0] & 0x08) currentDoors |= 0x10; // AR Droite
            if (rxFrame.data[0] & 0x10) currentDoors |= 0x08; // Coffre
            
            fuelLevel = map(rxFrame.data[1], 0, 255, 0, 100);

            // On garde le bit 0x01 ici, mais RadioSend le filtrera pour le poste
            if (rxFrame.data[2] & 0x01) currentDoors |= 0x01; 
            break;

        case 0x54C: // --- ORDINATEUR DE BORD ---
            fuelConsoMoy = rxFrame.data[2] * 0.1f; 
            dteValue = (rxFrame.data[4] << 8) | rxFrame.data[5]; 
            break;

        case 0x5E5: // --- ÉLECTRIQUE (Batterie) ---
            voltBat = rxFrame.data[0] * 0.1f;
            break;

        case 0x510: // --- CLIMATISATION (Température ext) ---
            tempExt = (int8_t)rxFrame.data[0] - 40;
            break;
    }

    // --- AFFICHAGE DES LOGS (Seulement si Serial est connecté) ---
    if (Serial && (now - lastLogTime >= 1000)) {
        Serial.println("--- NISSAN DATA DECODED ---");
        Serial.printf("RPM: %u tr/min | Speed: %u km/h | Volt: %.1fV\n", engineRPM, vehicleSpeed, voltBat);
        Serial.printf("Temp: %d°C | Fuel: %u%% | DTE: %d km\n", tempExt, fuelLevel, dteValue);
        Serial.printf("Steer: %d | Doors Hex: 0x%02X | Handbrake: %s\n", 
                      currentSteer, currentDoors & 0xFE, (currentDoors & 0x01) ? "ON" : "OFF");
        Serial.println("---------------------------");
        lastLogTime = now;
    }
}