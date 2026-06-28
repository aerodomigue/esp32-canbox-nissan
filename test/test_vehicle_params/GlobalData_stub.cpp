// Stub definitions for GlobalData globals used by CanConfigProcessor::writeToGlobalData()
#include <stdint.h>
#include "Arduino.h"
#include "LittleFS.h"

int16_t  currentSteer       = 0;
uint16_t engineRPM          = 0;
uint8_t  vehicleSpeed       = 0;
uint8_t  currentDoors       = 0;
uint8_t  fuelLevel          = 0;
float    voltBat             = 0.0f;
int16_t  dteValue           = 0;
float    fuelConsoMoy        = 0.0f;
int8_t   tempExt             = 0;
int8_t   coolantTemp         = 0;
uint32_t currentOdo          = 0;

bool         indicatorLeft           = false;
bool         indicatorRight          = false;
bool         headlightsOn            = false;
bool         highBeamOn              = false;
bool         parkingLightsOn         = false;
unsigned long lastLeftIndicatorTime  = 0;
unsigned long lastRightIndicatorTime = 0;

uint16_t fuelConsumptionInst = 0;
uint16_t fuelConsumptionAvg  = 0;
uint16_t averageSpeed        = 0;
uint16_t elapsedTime         = 0;

// Arduino and filesystem globals (SerialClass and FS are defined in Arduino.h/LittleFS.h mocks)
SerialClass Serial;
FS LittleFS;
