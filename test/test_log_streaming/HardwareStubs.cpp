#include "Arduino.h"
#include "ESP32-TWAI-CAN.hpp"
#include "LittleFS.h"
#include "Update.h"
#include "base64.h"

SerialClass Serial;
EspClass ESP;
SimulatedCan ESP32Can;
FS LittleFS;
UpdateClass Update;

// No OTA/upload payload is exercised by this streaming suite.
size_t base64Decode(const char*, uint8_t*, size_t) { return 0; }
