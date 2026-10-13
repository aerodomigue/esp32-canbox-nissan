#include "UsbConsole.h"
#include <Arduino.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

UsbConsole Console;

void UsbConsole::reset() {
    _head = _tail = _used = 0;
    _droppedRaw = _droppedDiagnostic = _shortWrites = _rejectedResponses = 0;
    _commandActive = _commandOverflow = false;
    _commandTail = _commandUsed = 0;
}

void UsbConsole::begin() {
    reset();
    // These setters replace existing driver buffers: call before begin/output.
    Serial.setRxBufferSize(1024);
    Serial.setTxBufferSize(4096);
    Serial.setTxTimeoutMs(0);
    Serial.begin(115200);
}

void UsbConsole::append(const uint8_t* bytes, size_t length) {
    const size_t first = length < CAPACITY - _tail ? length : CAPACITY - _tail;
    memcpy(_buffer + _tail, bytes, first);
    memcpy(_buffer, bytes + first, length - first);
    _tail = (_tail + length) % CAPACITY;
    _used += length;
}

bool UsbConsole::logLine(const char* text, bool diagnostic) {
    if (!text) return false;
    const size_t length = strnlen(text, MAX_LOG_LENGTH + 1);
    const size_t reserve = COMMAND_RESERVE + (diagnostic ? 0 : DIAGNOSTIC_RESERVE);
    // Reject an entire record before changing the FIFO. Never evict old bytes,
    // including a line whose prefix may already have reached the USB driver.
    if (length > MAX_LOG_LENGTH || CAPACITY - _used < length + 2 + reserve) {
        if (diagnostic) ++_droppedDiagnostic;
        else ++_droppedRaw;
        return false;
    }
    append(reinterpret_cast<const uint8_t*>(text), length);
    append(reinterpret_cast<const uint8_t*>("\r\n"), 2);
    return true;
}

size_t UsbConsole::write(const uint8_t* bytes, size_t length) {
    if (!bytes || !length) return 0;
    if (_commandOverflow || length > CAPACITY - _used) {
        if (_commandActive) _commandOverflow = true;
        else ++_rejectedResponses;
        return 0;
    }
    append(bytes, length);
    return length;
}

size_t UsbConsole::print(const char* text) {
    return text ? write(reinterpret_cast<const uint8_t*>(text), strlen(text)) : 0;
}

size_t UsbConsole::print(int value) { return printf("%d", value); }
size_t UsbConsole::println() { return print("\r\n"); }
size_t UsbConsole::println(const char* text) { const size_t n = print(text); return n + println(); }
size_t UsbConsole::println(char c) { const size_t n = print(c); return n + println(); }

size_t UsbConsole::printf(const char* format, ...) {
    char local[256];
    va_list args;
    va_start(args, format);
    va_list copy;
    va_copy(copy, args);
    const int length = vsnprintf(local, sizeof(local), format, copy);
    va_end(copy);
    if (length < 0) { va_end(args); return 0; }
    if (static_cast<size_t>(length) < sizeof(local)) {
        va_end(args);
        return write(reinterpret_cast<const uint8_t*>(local), length);
    }
    // Long command output is exceptional (e.g. a profile name). Never truncate
    // it silently or allocate more memory than the FIFO can accept.
    if (_commandOverflow || static_cast<size_t>(length) > CAPACITY - _used) {
        if (_commandActive) _commandOverflow = true;
        else ++_rejectedResponses;
        va_end(args);
        return 0;
    }
    char* extended = static_cast<char*>(malloc(static_cast<size_t>(length) + 1));
    if (!extended) {
        if (_commandActive) _commandOverflow = true;
        else ++_rejectedResponses;
        va_end(args);
        return 0;
    }
    vsnprintf(extended, static_cast<size_t>(length) + 1, format, args);
    va_end(args);
    const size_t written = write(reinterpret_cast<const uint8_t*>(extended), length);
    free(extended);
    return written;
}

void UsbConsole::beginCommand() {
    _commandActive = true;
    _commandOverflow = false;
    _commandTail = _tail;
    _commandUsed = _used;
}

void UsbConsole::endCommand() {
    if (!_commandActive) return;
    _commandActive = false;
    if (_commandOverflow) {
        // Nothing in this response has been drained while the transaction was
        // open. Replace it with one complete error, preserving older logs.
        _tail = _commandTail;
        _used = _commandUsed;
        _commandOverflow = false;
        ++_rejectedResponses;
        println("ERROR: Response exceeds USB buffer or allocation failed");
    }
}

void UsbConsole::drain() {
    if (_commandActive || !_used || !Serial) return;
    size_t budget = DRAIN_BUDGET;
    for (unsigned call = 0; call < 8 && _used && budget; ++call) {
        const int available = Serial.availableForWrite();
        if (available <= 0) break;
        size_t count = _used;
        if (count > CAPACITY - _head) count = CAPACITY - _head;
        if (count > static_cast<size_t>(available)) count = available;
        if (count > budget) count = budget;
        const size_t written = Serial.write(_buffer + _head, count);
        if (written < count) ++_shortWrites;
        if (!written) break;
        // HWCDC returns the number accepted. Advance only over those bytes;
        // the unsent suffix precedes every subsequent record/command response.
        _head = (_head + written) % CAPACITY;
        _used -= written;
        budget -= written;
    }
}

void UsbConsole::drainBeforeRestart(uint32_t milliseconds) {
    endCommand();
    // The previous reboot delay is retained while servicing the software FIFO.
    // Use a bounded iteration count also when a native mock has a fixed clock.
    for (uint32_t i = 0; i < milliseconds; ++i) {
        drain();
        delay(1);
    }
}
