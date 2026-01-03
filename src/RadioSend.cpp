#include <Arduino.h>
#include "GlobalData.h"

extern HardwareSerial RadioSerial;

void transmettreVersPoste(uint8_t commande, uint8_t* donnees, uint8_t longueur) {
    uint8_t trame[longueur + 4];
    trame[0] = 0x2E; 
    trame[1] = commande;
    trame[2] = longueur;
    uint8_t checksum = commande + longueur;
    for (int i = 0; i < longueur; i++) {
        trame[3 + i] = donnees[i];
        checksum += donnees[i];
    }
    trame[3 + longueur] = checksum ^ 0xFF;
    RadioSerial.write(trame, longueur + 4);
}

void processRadioUpdates() {
    static unsigned long lastDyn = 0;
    static unsigned long lastTel = 0;
    unsigned long now = millis();

    // -------------------------------------------------------------------------
    // FLUX RAPIDE (50ms) - Pour la réactivité visuelle
    // -------------------------------------------------------------------------
    if (now - lastDyn >= 50) {
        // -- Angle Volant (Lignes de trajectoire caméra)
        uint8_t payloadSteer[2] = { (uint8_t)(currentSteer & 0xFF), (uint8_t)(currentSteer >> 8) };
        transmettreVersPoste(0x29, payloadSteer, 2);

        // -- RPM moteur (Aiguille du compte-tours)
        uint8_t payloadRPM[2] = { (uint8_t)(engineRPM >> 8), (uint8_t)(engineRPM & 0xFF) };
        transmettreVersPoste(0x26, payloadRPM, 2);
        
        lastDyn = now;
    }

    // -------------------------------------------------------------------------
    // FLUX LENT (800ms) - Pour les données d'état
    // -------------------------------------------------------------------------
    if (now - lastTel >= 800) {
        // -- États des Ouvrants (Portes, Coffre) et Sécurité (Frein à main)
        transmettreVersPoste(0x24, &currentDoors, 1);

        // -- Système complet (Trame 0x27 multi-données)
        // Byte 0: fuelLevel (Niveau essence)
        // Byte 1: voltBat*10 (Tension batterie)
        // Byte 2: tempExt+40 (Température extérieure avec offset)
        // Byte 3: dteValue >> 8 (Autonomie restante - High Byte)
        // Byte 4: dteValue & 0xFF (Autonomie restante - Low Byte)
        // Byte 5: fuelConsoMoy*10 (Consommation moyenne)
        // Byte 6: vehicleSpeed (Vitesse instantanée)
        uint8_t payloadSys[7] = { 
            fuelLevel,                           // (Essence)
            (uint8_t)(voltBat * 10),             // (Batterie)
            (uint8_t)(tempExt + 40),             // (Température Ext)
            (uint8_t)(dteValue >> 8),            // (DTE High)
            (uint8_t)(dteValue & 0xFF),          // (DTE Low)
            (uint8_t)(fuelConsoMoy * 10),        // (Conso Moyenne)
            vehicleSpeed                         // (Vitesse)
        };
        transmettreVersPoste(0x27, payloadSys, 7);
        
        lastTel = now;
    }
}