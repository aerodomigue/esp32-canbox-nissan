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
    bool endsWith(const char* suffix) const {
        size_t sl = strlen(suffix);
        return _s.size() >= sl && _s.compare(_s.size()-sl, sl, suffix) == 0;
    }
    bool endsWith(const String& s) const { return endsWith(s.c_str()); }
    bool operator==(const char* s)  const { return _s == (s ? s : ""); }
    bool operator!=(const char* s)  const { return !(*this == s); }
    bool operator==(const String& o) const { return _s == o._s; }
    bool operator!=(const String& o) const { return _s != o._s; }
private:
    std::string _s;
};

// Serial stub — routes to stdout, captures nothing
struct SerialClass {
    void begin(int) {}
    template<typename... Args>
    void printf(const char* fmt, Args... args) { ::printf(fmt, args...); }  // NOLINT
    void println(const char* s)  { ::puts(s ? s : ""); }
    void println(char c)         { ::putchar(c); ::putchar('\n'); }
    void println()               { ::putchar('\n'); }
    void print(const char* s)    { ::fputs(s ? s : "", stdout); }
    void print(char c)           { ::putchar(c); }
    void print(int v)            { ::printf("%d", v); }
    size_t write(uint8_t b)             { ::putchar(b); return 1; }
    size_t write(const uint8_t* b, size_t n) { for (size_t i=0;i<n;i++) ::putchar(b[i]); return n; }
    int  available() { return 0; }
    int  read()      { return -1; }
    void flush()     {}
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

// millis() / delay() stubs
inline unsigned long millis() { return 0; }
inline void delay(unsigned long) {}

// ESP object stub
struct EspClass {
    uint32_t    getFreeHeap()         { return 200000; }
    uint32_t    getFreeSketchSpace()  { return 1048576; }
    uint32_t    getSketchSize()       { return 512000; }
    uint32_t    getCpuFreqMHz()       { return 160; }
    const char* getChipModel()        { return "ESP32-C3"; }
    uint32_t    getChipRevision()     { return 3; }
    void        restart()             {}
};
extern EspClass ESP;

// Arduino map() — linear range mapping
inline long map(long x, long in_min, long in_max, long out_min, long out_max) {
    if (in_max == in_min) return out_min;
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}
