// Minimal base64 stub for SerialCommand test build
// The real base64.cpp depends on libb64 (ESP32 library) — not available on native.
// OTA DATA commands are not tested here, so a stub that returns 0 is sufficient.
#include "../../include/base64.h"

size_t base64Decode(const char* /*input*/, uint8_t* /*output*/, size_t /*maxLen*/) {
    return 0;
}
