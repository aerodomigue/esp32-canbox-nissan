#include "GlobalData.h"

// Ici, on déclare les variables SANS "extern" pour les créer en mémoire.
// On les initialise à 0 pour éviter les valeurs aléatoires au démarrage.

int16_t currentSteer = 0;
uint16_t engineRPM = 0;
uint8_t vehicleSpeed = 0;
uint8_t currentDoors = 0;
uint8_t fuelLevel = 0;
float voltBat = 0.0;     // Initialisation flottante
int16_t dteValue = 0;
float fuelConsoMoy = 0.0;
int8_t tempExt = 0;