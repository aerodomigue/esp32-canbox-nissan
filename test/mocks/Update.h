#pragma once
#include <stdint.h>
#include <stddef.h>
#include "Arduino.h"

class UpdateClass {
public:
    bool   begin(size_t = 0)               { return true; }
    size_t write(const uint8_t*, size_t n) { return n; }
    bool   end(bool = false)               { return true; }
    void   abort()                         {}
    bool   isRunning()                     { return false; }
    const char* errorString()              { return "none"; }
    bool   hasError()                      { return false; }
};

extern UpdateClass Update;
