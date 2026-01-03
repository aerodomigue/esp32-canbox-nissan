#include <ESP32-TWAI-CAN.hpp>
#include "GlobalData.h"

#define LED_HEARTBEAT 8

void handleCanCapture(CanFrame &rxFrame) {
    switch (rxFrame.identifier) {
        
        case 0x002: // --- DIRECTION (Angle Volant) ---
            // Octets 0 et 1 : Valeur brute signée (Big Endian)
            // Heartbeat : Toggle la LED à chaque mouvement détecté
            digitalWrite(LED_HEARTBEAT, !digitalRead(LED_HEARTBEAT)); 
            currentSteer = (int16_t)((rxFrame.data[0] << 8) | rxFrame.data[1]);
            currentSteer = map(currentSteer, -4000, 4000, -500, 500);
            break;

        case 0x156: // --- MOTEUR (Régime RPM) ---
            // Octets 1 et 2 : (RPM * 8)
            engineRPM = ((rxFrame.data[1] << 8) | rxFrame.data[2]) / 8;
            break;

        case 0x284: // --- ROUES (Vitesse véhicule) ---
            // Octets 0 et 1 : (Vitesse en km/h * 100)
            vehicleSpeed = ((rxFrame.data[0] << 8) | rxFrame.data[1]) / 100;
            break;

        case 0x60D: // --- CARROSSERIE (BCM) ---
            // Octet 0 : Masques binaires pour les ouvrants
            // Octet 1 : Niveau de carburant
            // Octet 2 : État du frein à main
            currentDoors = 0;
            if (rxFrame.data[0] & 0x01) currentDoors |= 0x80; // (Porte Conducteur)
            if (rxFrame.data[0] & 0x02) currentDoors |= 0x40; // (Porte Passager)
            if (rxFrame.data[0] & 0x04) currentDoors |= 0x20; // (Arrière Gauche)
            if (rxFrame.data[0] & 0x08) currentDoors |= 0x10; // (Arrière Droite)
            if (rxFrame.data[0] & 0x10) currentDoors |= 0x08; // (Coffre)
            
            fuelLevel = rxFrame.data[1];                      // (Niveau Essence 0-255)

            if (rxFrame.data[2] & 0x01) currentDoors |= 0x01; // (Frein à main tiré)
            break;

        case 0x54C: // --- ORDINATEUR DE BORD ---
            // Octet 2 : Consommation (L/100 * 10)
            // Octets 4 et 5 : Autonomie (Distance To Empty)
            fuelConsoMoy = rxFrame.data[2] * 0.1; 
            dteValue = (rxFrame.data[4] << 8) | rxFrame.data[5]; 
            break;

        case 0x5E5: // --- ÉLECTRIQUE (Batterie) ---
            // Octet 0 : Tension batterie (Volts * 10)
            voltBat = rxFrame.data[0] * 0.1;
            break;

        case 0x510: // --- CLIMATISATION (Température) ---
            // Octet 0 : Température extérieure (Celsius + 40)
            tempExt = rxFrame.data[0] - 40;
            break;
    }
}