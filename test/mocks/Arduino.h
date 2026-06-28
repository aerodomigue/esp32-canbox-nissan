#pragma once
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <string>
#include <vector>

// Arduino String type — minimal stub for native builds
class String {
public:
    String() {}
    String(const char* s) : _s(s ? s : "") {}
    String(const String& o) : _s(o._s) {}
    String& operator=(const char* s) { _s = s ? s : ""; return *this; }
    String& operator=(const String& o) { _s = o._s; return *this; }
    const char* c_str() const { return _s.c_str(); }
    bool isEmpty() const { return _s.empty(); }
    size_t length() const { return _s.size(); }
private:
    std::string _s;
};

// Serial stub — printf to stdout
struct SerialClass {
    void begin(int) {}
    template<typename... Args>
    void printf(const char* fmt, Args... args) { ::printf(fmt, args...); }  // NOLINT
    void println(const char* s) { ::puts(s); }
    void print(const char* s) { ::fputs(s, stdout); }
};

extern SerialClass Serial;

// HardwareSerial — captures written bytes for test assertions
class HardwareSerial {
public:
    std::vector<uint8_t> captured;
    void begin(int) {}
    size_t write(uint8_t b)                        { captured.push_back(b); return 1; }
    size_t write(const uint8_t* buf, size_t len)   { for (size_t i=0;i<len;i++) captured.push_back(buf[i]); return len; }
    int    available()                             { return 0; }
    int    read()                                  { return -1; }
    void   clear()                                 { captured.clear(); }
};

// Arduino type aliases
typedef unsigned char  byte;
typedef unsigned int   word;
typedef bool           boolean;

// Arduino utility macros
// In C++ mode use std::min/std::max to avoid macro conflicts with GCC 13 STL
// (function-like macros break stl_bvector.h template instantiation under GCC ≥ 13)
#ifdef __cplusplus
#include <algorithm>
using std::min;
using std::max;
#else
#ifndef min
#define min(a,b) ((a)<(b)?(a):(b))
#endif
#ifndef max
#define max(a,b) ((a)>(b)?(a):(b))
#endif
#endif

// millis() stub
inline unsigned long millis() { return 0; }

// Arduino map() — linear range mapping
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
