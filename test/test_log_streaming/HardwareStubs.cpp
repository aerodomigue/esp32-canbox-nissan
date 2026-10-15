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

// Explicit fixture for OTA transport/ACK tests, not a test of the Base64 decoder.
// Keep native CI independent of the ESP32 framework/libb64 installation.
size_t base64Decode(const char* input, uint8_t* output, size_t maxLen) {
    if (!input || !output || maxLen < 4 || strcmp(input, "AQIDBA==") != 0) return 0;
    const uint8_t fixture[] = {1, 2, 3, 4};
    memcpy(output, fixture, sizeof(fixture));
    return sizeof(fixture);
}
