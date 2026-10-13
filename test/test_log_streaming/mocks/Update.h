#pragma once
#include "Arduino.h"
// Flash writes are simulated; OTA tests verify parsing and ordered ACKs only.
class UpdateClass {
public:
    std::vector<uint8_t> received;
    bool running = false;
    bool begin(size_t = 0) { received.clear(); running = true; return true; }
    size_t write(const uint8_t* data, size_t size) {
        received.insert(received.end(), data, data + size);
        return size;
    }
    bool end(bool = false) { return true; }
    void abort() { running = false; }
    bool isRunning() const { return running; }
    const char* errorString() const { return "none"; }
    bool hasError() const { return false; }
};
extern UpdateClass Update;
