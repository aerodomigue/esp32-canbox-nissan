#pragma once
#include "Arduino.h"

struct CanFrame {
    uint32_t identifier;
    uint8_t data_length_code;
    uint8_t data[8];
    bool extd;
    bool rtr;
};

constexpr int TWAI_STATE_RUNNING = 1;
constexpr int TWAI_STATE_BUS_OFF = 2;
struct SimulatedRead { CanFrame frame; uint32_t at; };

class SimulatedCan {
public:
    std::deque<CanFrame> queue;
    std::vector<SimulatedRead> reads;
    std::vector<CanFrame> transmitted;
    std::vector<uint32_t> readTimeouts;
    std::vector<uint32_t> writeTimeouts;
    uint32_t missed = 0;
    uint32_t failed = 0;
    size_t capacity = 128;
    unsigned speed = 0;
    unsigned convertArgument = 0;
    int txPin = -1;
    int rxPin = -1;
    bool started = false;

    void reset() { *this = SimulatedCan{}; }
    bool enqueue(const CanFrame& frame) {
        if (queue.size() >= capacity) { ++missed; return false; }
        queue.push_back(frame);
        return true;
    }
    bool readFrame(CanFrame& frame, uint32_t timeout) {
        readTimeouts.push_back(timeout);
        if (queue.empty()) return false;
        frame = queue.front();
        queue.pop_front();
        reads.push_back({frame, StreamingSimulation::now});
        return true;
    }
    bool writeFrame(const CanFrame& frame, uint32_t timeout) {
        writeTimeouts.push_back(timeout);
        transmitted.push_back(frame);
        return true;
    }
    void setPins(int tx, int rx) { txPin = tx; rxPin = rx; }
    unsigned convertSpeed(unsigned kbps) { convertArgument = kbps; return kbps; }
    void setSpeed(unsigned value) { speed = value; }
    void setRxQueueSize(unsigned value) { capacity = value; }
    bool begin() { started = true; return true; }
    uint32_t rxErrorCounter() const { return 0; }
    uint32_t busErrCounter() const { return 0; }
    uint32_t rxMissedCounter() const { return missed; }
    uint32_t txFailedCounter() const { return failed; }
    uint32_t inRxQueue() const { return static_cast<uint32_t>(queue.size()); }
    int canState() const { return TWAI_STATE_RUNNING; }
};
extern SimulatedCan ESP32Can;
