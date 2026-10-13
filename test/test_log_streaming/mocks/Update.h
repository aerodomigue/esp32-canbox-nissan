#pragma once
#include "Arduino.h"
// OTA is not exercised here; provide the interface required by SerialCommand.
class UpdateClass {
public:
    bool begin(size_t = 0) { return true; }
    size_t write(const uint8_t*, size_t size) { return size; }
    bool end(bool = false) { return true; }
    void abort() {}
    bool isRunning() const { return false; }
    const char* errorString() const { return "none"; }
    bool hasError() const { return false; }
};
extern UpdateClass Update;
