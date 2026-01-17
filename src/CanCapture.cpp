#include <ESP32-TWAI-CAN.hpp>
#include "GlobalData.h"

#define LED_HEARTBEAT 8

void handleCanCapture(CanFrame &rxFrame) {
    switch (rxFrame.identifier) {
        
        case 0x002: // --- DIRECTION (Angle Volant) ---
            digitalWrite(LED_HEARTBEAT, !digitalRead(LED_HEARTBEAT)); 
            // On garde la valeur brute signée, on la traitera dans RadioSend
            currentSteer = (int16_t)((rxFrame.data[0] << 8) | rxFrame.data[1]);
            break;

        case 0x156: // --- MOTEUR (Régime RPM) ---
            engineRPM = ((rxFrame.data[1] << 8) | rxFrame.data[2]) / 8;
            break;

        case 0x284: // --- ROUES (Vitesse véhicule) ---
            // On stocke la vitesse en km/h
            vehicleSpeed = ((rxFrame.data[0] << 8) | rxFrame.data[1]) / 100;
            break;

        case 0x60D: // --- CARROSSERIE (BCM) ---
            currentDoors = 0;
            if (rxFrame.data[0] & 0x01) currentDoors |= 0x80; // Conducteur
            if (rxFrame.data[0] & 0x02) currentDoors |= 0x40; // Passager
            if (rxFrame.data[0] & 0x04) currentDoors |= 0x20; // AR Gauche
            if (rxFrame.data[0] & 0x08) currentDoors |= 0x10; // AR Droite
            if (rxFrame.data[0] & 0x10) currentDoors |= 0x08; // Coffre
            
            // CONVERSION NISSAN (0-255) -> VW (0-100%)
            fuelLevel = map(rxFrame.data[1], 0, 255, 0, 100);

            if (rxFrame.data[2] & 0x01) currentDoors |= 0x01; // Frein à main
            break;

        case 0x54C: // --- ORDINATEUR DE BORD ---
            fuelConsoMoy = rxFrame.data[2] * 0.1f; 
            dteValue = (rxFrame.data[4] << 8) | rxFrame.data[5]; 
            break;

        case 0x5E5: // --- ÉLECTRIQUE (Batterie) ---
            // Capture précise : 142 pour 14.2V
            voltBat = rxFrame.data[0] * 0.1f;
            break;

        case 0x510: // --- CLIMATISATION (Température) ---
            // Format Nissan standard : Celsius + 40
            tempExt = (float)rxFrame.data[0] - 40.0f;
            break;
    }
}