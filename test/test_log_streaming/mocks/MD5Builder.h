#pragma once
#include "Arduino.h"
// OTA is not exercised here; no MD5 correctness claim is made by this mock.
class MD5Builder {
public:
    void begin() {}
    void add(const uint8_t*, size_t) {}
    void add(const char*) {}
    void calculate() {}
    String toString() const { return String("00000000000000000000000000000000"); }
};
