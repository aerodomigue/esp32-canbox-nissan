#pragma once

#include <stddef.h>
#include <stdint.h>

// Single producer: the Arduino loop task. All USB output uses this FIFO;
// only drain() touches the physical writer. No allocation in the capture path.
class UsbConsole {
public:
    static constexpr size_t CAPACITY = 64 * 1024;
    static constexpr size_t COMMAND_RESERVE = 12 * 1024;
    static constexpr size_t DIAGNOSTIC_RESERVE = 512;
    static constexpr size_t DRAIN_BUDGET = 4096;
    static constexpr size_t MAX_LOG_LENGTH = 255;

    void begin();
    void reset();
    void drain();
    // A flush is a bounded pump, never a wait for the host to consume bytes.
    void flush() { drain(); }
    // Used only before an intentional reboot, replacing its existing delay.
    void drainBeforeRestart(uint32_t milliseconds);

    bool logLine(const char* text, bool diagnostic = false);
    size_t write(const uint8_t* bytes, size_t length);
    size_t write(uint8_t byte) { return write(&byte, 1); }
    size_t print(const char* text);
    size_t print(char c) { return write(static_cast<uint8_t>(c)); }
    size_t print(int value);
    size_t println(const char* text);
    size_t println(char c);
    size_t println();
    size_t printf(const char* format, ...) __attribute__((format(printf, 2, 3)));

    bool canProcessCommand() const { return CAPACITY - _used >= COMMAND_RESERVE; }
    void beginCommand();
    void endCommand();
    size_t queuedBytes() const { return _used; }
    uint32_t droppedRawLines() const { return _droppedRaw; }
    uint32_t droppedDiagnosticLines() const { return _droppedDiagnostic; }
    uint32_t shortWrites() const { return _shortWrites; }
    uint32_t rejectedResponses() const { return _rejectedResponses; }

private:
    uint8_t _buffer[CAPACITY] = {};
    size_t _head = 0;
    size_t _tail = 0;
    size_t _used = 0;
    uint32_t _droppedRaw = 0;
    uint32_t _droppedDiagnostic = 0;
    uint32_t _shortWrites = 0;
    uint32_t _rejectedResponses = 0;
    bool _commandActive = false;
    bool _commandOverflow = false;
    size_t _commandTail = 0;
    size_t _commandUsed = 0;
    void append(const uint8_t* bytes, size_t length);
};

extern UsbConsole Console;
