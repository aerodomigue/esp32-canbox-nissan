#pragma once

#include <ESP32-TWAI-CAN.hpp>
#include <stdint.h>

// Read-only service 01 measurements and optional, bounded Nissan service22 probes.
// They never update vehicle data. F15-only probes use short, bounded sessions.
// Wire transmission and log output are injected for native tests.
class ObdCalibration {
public:
    using Send = bool (*)(const CanFrame&, void*);
    using Emit = void (*)(const char*, void*);
    static constexpr uint32_t REQUEST_INTERVAL_MS = 500;
    static constexpr uint32_t RESPONSE_WINDOW_MS = 250;
    static constexpr uint32_t DISCOVERY_RETRY_MS = 5000;

    ObdCalibration(Send send, Emit emit, void* context = nullptr);
    void setEnabled(bool enabled, uint32_t now);
    void setNissanProbesEnabled(bool enabled);
    void setNissanProbesEnabled(bool enabled, uint32_t now);
    void tick(uint32_t now);
    void receive(const CanFrame& frame, uint32_t now);
    bool enabled() const { return _enabled; }
    bool waiting() const { return _pending; }

private:
    enum class Stage { Page00, Page20, Page40, Poll, Retry };
    enum class NissanStep { Idle, Open, First, Second, Close };
    struct Ecu {
        uint32_t support[3] = {};
        bool valid[3] = {};
    };
    Send _send;
    Emit _emit;
    void* _context;
    Ecu _ecus[8] = {};
    bool _enabled = false;
    bool _pending = false;
    bool _nissanProbesEnabled = false;
    bool _isProbe = false;
    bool _isSession = false;
    bool _sessionMayBeOpen = false;
    bool _nissanBlocked = false;
    NissanStep _nissanStep = NissanStep::Idle;
    bool _probeTerminalNegative = false;
    uint8_t _probeLastNrc = 0;
    Stage _stage = Stage::Page00;
    uint8_t _pid = 0;
    uint8_t _responses = 0;
    uint8_t _positives = 0;
    uint8_t _negatives = 0;
    uint8_t _attempts = 0;
    uint8_t _pollIndex = 0;
    uint8_t _fastIndex = 0;
    uint8_t _scheduleSlot = 0;
    uint8_t _nissanGroup = 0;
    uint8_t _completedGroups = 0;
    uint8_t _sessionCommand = 0;
    uint8_t _activeProbe = 0;
    uint16_t _requestId = 0x7DF;
    uint32_t _sequence = 0;
    uint32_t _sentAt = 0;
    bool _hasAttempt = false;
    uint32_t _lastAttempt = 0;
    uint32_t _retryAt = 0;
    uint32_t _lastGroupAttempt = 0;
    uint32_t _groupStarted = 0;
    uint32_t _lastNow = 0;

    void log(const char* format, ...);
    bool supports(uint8_t ecu, uint8_t pid) const;
    bool anySupports(uint8_t pid) const;
    void finish(uint32_t now);
    void request(uint8_t pid, uint16_t id, uint32_t now);
    bool poll(bool fast, bool skipFast, uint32_t now);
    bool pollProbe(uint32_t now);
    void requestProbe(uint8_t index, uint32_t now);
    void requestSession(uint8_t command, uint32_t now);
    void finishNissan(uint32_t now);
    void cancelNissan(uint32_t now, const char* reason);
    void runNissan(uint32_t now);
};
