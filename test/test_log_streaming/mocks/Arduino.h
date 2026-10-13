#pragma once

#include <algorithm>
#include <cctype>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <functional>
#include <string>
#include <vector>

// All time and USB behavior below are deterministic simulations, not CDC measurements.
namespace StreamingSimulation {
inline uint32_t now = 0;
inline std::function<void(uint32_t, uint32_t)> onAdvance;
inline void advance(uint32_t ms) {
    const uint32_t before = now;
    now += ms;
    if (onAdvance) onAdvance(before, ms);
}
inline unsigned gpio[32] = {};
inline unsigned watchdogResets = 0;
}

class String {
public:
    String() = default;
    String(const char* value) : _value(value ? value : "") {}
    String& operator=(const char* value) { _value = value ? value : ""; return *this; }
    const char* c_str() const { return _value.c_str(); }
    bool isEmpty() const { return _value.empty(); }
    size_t length() const { return _value.size(); }
    bool endsWith(const char* suffix) const {
        const size_t n = strlen(suffix);
        return _value.size() >= n && _value.compare(_value.size() - n, n, suffix) == 0;
    }
    bool endsWith(const String& suffix) const { return endsWith(suffix.c_str()); }
    bool operator==(const char* value) const { return _value == (value ? value : ""); }
    bool operator!=(const char* value) const { return !(*this == value); }
    bool operator==(const String& value) const { return _value == value._value; }
    bool operator!=(const String& value) const { return !(*this == value); }
private:
    std::string _value;
};

struct SimulatedWrite {
    std::string attempted;
    size_t accepted;
    uint32_t at;
};

class SerialClass {
public:
    std::string captured;
    std::deque<char> input;
    std::vector<SimulatedWrite> writes;
    std::string shortPrefix;
    size_t shortLimit = 0;
    std::string blockingPrefix;
    uint32_t blockingMs = 0;
    unsigned begins = 0;

    void begin(unsigned long) { ++begins; }
    void inject(const char* text) { while (*text) input.push_back(*text++); }
    int available() const { return static_cast<int>(input.size()); }
    int read() {
        if (input.empty()) return -1;
        const unsigned char c = input.front();
        input.pop_front();
        return c;
    }
    size_t write(const uint8_t* bytes, size_t n) {
        const std::string attempted(reinterpret_cast<const char*>(bytes), n);
        const uint32_t startedAt = StreamingSimulation::now;
        if (!blockingPrefix.empty() && attempted.rfind(blockingPrefix, 0) == 0) {
            blockingPrefix.clear();  // Only the selected first write stalls.
            StreamingSimulation::advance(blockingMs);
        }
        size_t accepted = n;
        if (!shortPrefix.empty() && attempted.rfind(shortPrefix, 0) == 0) {
            shortPrefix.clear();  // Only the selected first write is incomplete.
            accepted = std::min(n, shortLimit);
        }
        captured.append(attempted, 0, accepted);
        writes.push_back({attempted, accepted, startedAt});
        return accepted;
    }
    size_t write(uint8_t byte) { return write(&byte, 1); }
    size_t print(const char* text) {
        return write(reinterpret_cast<const uint8_t*>(text), strlen(text));
    }
    size_t print(char c) { return write(static_cast<uint8_t>(c)); }
    size_t print(int value) { return printf("%d", value); }
    size_t println() { return print("\r\n"); }
    size_t println(const char* text) { const size_t n = print(text ? text : ""); return n + println(); }
    size_t println(char c) { const size_t n = print(c); return n + println(); }
    size_t printf(const char* format, ...) {
        va_list args;
        va_start(args, format);
        va_list sizing;
        va_copy(sizing, args);
        const int size = vsnprintf(nullptr, 0, format, sizing);
        va_end(sizing);
        if (size < 0) { va_end(args); return 0; }
        std::vector<char> buffer(static_cast<size_t>(size) + 1);
        vsnprintf(buffer.data(), buffer.size(), format, args);
        va_end(args);
        return write(reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<size_t>(size));
    }
    void flush() {}
    void reset() { *this = SerialClass{}; }
    void clearOutput() { captured.clear(); writes.clear(); }
};
extern SerialClass Serial;

class HardwareSerial {
public:
    explicit HardwareSerial(int = 0) {}
    std::vector<uint8_t> captured;
    void begin(unsigned long, int = 0, int = -1, int = -1) {}
    size_t write(uint8_t byte) { captured.push_back(byte); return 1; }
    size_t write(const uint8_t* bytes, size_t n) { captured.insert(captured.end(), bytes, bytes + n); return n; }
    int available() const { return 0; }
    int read() { return -1; }
    void clear() { captured.clear(); }
};

using byte = unsigned char;
using word = unsigned int;
using boolean = bool;
using std::min;
using std::max;
#define OUTPUT 1
#define HIGH 1
#define LOW 0
#define SERIAL_8N1 0
inline unsigned long millis() { return StreamingSimulation::now; }
inline void delay(unsigned long ms) { StreamingSimulation::advance(static_cast<uint32_t>(ms)); }
inline void pinMode(unsigned, unsigned) {}
inline void digitalWrite(unsigned pin, unsigned value) { StreamingSimulation::gpio[pin] = value; }
inline unsigned digitalRead(unsigned pin) { return StreamingSimulation::gpio[pin]; }
inline long map(long x, long inMin, long inMax, long outMin, long outMax) {
    return inMax == inMin ? outMin : (x - inMin) * (outMax - outMin) / (inMax - inMin) + outMin;
}

struct EspClass {
    unsigned restarts = 0;
    uint32_t getFreeHeap() const { return 200000; }
    uint32_t getFreeSketchSpace() const { return 1048576; }
    uint32_t getSketchSize() const { return 512000; }
    uint32_t getCpuFreqMHz() const { return 160; }
    const char* getChipModel() const { return "ESP32-C3-simulated"; }
    uint32_t getChipRevision() const { return 3; }
    void restart() { ++restarts; }
};
extern EspClass ESP;
