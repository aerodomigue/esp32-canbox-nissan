#pragma once
#include "Arduino.h"

class MD5Builder {
public:
    void   begin()                              {}
    void   add(const uint8_t*, size_t)          {}
    void   add(const char*)                     {}
    void   calculate()                          {}
    String toString()                           { return String("00000000000000000000000000000000"); }
};
