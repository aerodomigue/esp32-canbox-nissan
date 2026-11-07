#include "ObdCalibration.h"

#include <stdarg.h>
#include <stdio.h>

namespace {
struct Parameter {
    uint8_t pid;
    uint8_t bytes;
    const char* name;
    const char* unit;
};
constexpr Parameter PARAMETERS[] = {
    {0x0C, 2, "rpm", "rpm"},
    {0x0D, 1, "speed", "km/h"},
    {0x05, 1, "coolant", "degC"},
    {0x2F, 1, "fuel_level", "%"},
    {0x42, 2, "module_voltage", "V"},
    {0x46, 1, "ambient", "degC"},
    {0x0B, 1, "manifold_absolute_pressure", "kPa"},
    {0x5E, 2, "fuel_rate", "L/h"},
    {0x03, 2, "fuel_system_1", "bitmask"},
    {0x04, 1, "engine_load", "%"},
    {0x06, 1, "short_fuel_trim_b1", "%"},
    {0x07, 1, "long_fuel_trim_b1", "%"},
    {0x0F, 1, "intake_air_temperature", "degC"},
    {0x10, 2, "mass_air_flow", "g/s"},
    {0x11, 1, "throttle_position", "%"},
    {0x13, 1, "oxygen_sensors_present", "bitmask"},
    {0x24, 4, "wideband_equivalence_ratio_1", "ratio"},
    {0x44, 2, "commanded_equivalence_ratio", "ratio"},
    {0x51, 1, "fuel_type", "enum"},
    {0x5C, 1, "engine_oil_temperature", "degC"},
};
constexpr uint8_t PARAMETER_COUNT = sizeof(PARAMETERS) / sizeof(PARAMETERS[0]);
// Two fast slots, then one ordinary slot. Repeated MAF entries provide the
// independent fuel estimate with more samples, without starving slow readings.
constexpr uint8_t FAST_PIDS[] = {0x10, 0x03, 0x10, 0x24, 0x10, 0x44};
constexpr uint8_t FAST_COUNT = sizeof(FAST_PIDS) / sizeof(FAST_PIDS[0]);
bool isFast(uint8_t pid) {
    for (const auto fast : FAST_PIDS) if (pid == fast) return true;
    return false;
}
struct NissanProbe { uint16_t did; const char* name; };
constexpr NissanProbe NISSAN_PROBES[] = {
    {0x1101, "nissan_coolant_temperature_candidate"},
    {0x111F, "nissan_oil_temperature_candidate"},
    {0x1114, "nissan_fuel_level_raw"},
    {0x1183, "nissan_gear_position_raw"},
    {0x1304, "nissan_clutch_switch_raw"},
};
struct NissanGroup { uint8_t first; uint8_t count; };
constexpr NissanGroup NISSAN_GROUPS[] = {{0, 2}, {2, 2}, {4, 1}};
constexpr uint8_t GROUP_COUNT = sizeof(NISSAN_GROUPS) / sizeof(NISSAN_GROUPS[0]);
constexpr uint32_t GROUP_INTERVAL_MS = 10000;
// Close before a stalled scheduler can implicitly extend a proprietary session.
constexpr uint32_t GROUP_READ_LIMIT_MS = 2000;
const Parameter* parameter(uint8_t pid) {
    for (const auto& entry : PARAMETERS) if (entry.pid == pid) return &entry;
    return nullptr;
}
}

ObdCalibration::ObdCalibration(Send send, Emit emit, void* context)
    : _send(send), _emit(emit), _context(context) {}

void ObdCalibration::log(const char* format, ...) {
    if (!_emit) return;
    char line[256];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(line, sizeof(line), format, arguments);
    va_end(arguments);
    _emit(line, _context);
}

void ObdCalibration::setEnabled(bool enabled, uint32_t now) {
    _lastNow = now;
    if (_enabled == enabled) return;
    if (!enabled) cancelNissan(now, "disabled");
    _enabled = enabled;
    _pending = false;
    _stage = Stage::Page00;
    _attempts = _pollIndex = _fastIndex = _scheduleSlot = 0;
    _isProbe = _isSession = false;
    if (enabled) {
        _completedGroups = 0;
        _nissanBlocked = _sessionMayBeOpen = false;
        _nissanStep = NissanStep::Idle;
    }
    // Preserve the global wire-attempt budget across LOG OFF/ON, including a
    // best-effort 10 81 sent by cancelNissan(). Only the first attempt at boot
    // bypasses the interval; resetting discovery must not permit a burst.
    for (auto& ecu : _ecus) ecu = Ecu{};
    log("ODB STATE t_ms=%lu enabled=%u status=%s", static_cast<unsigned long>(now),
        enabled ? 1U : 0U, enabled ? "discovery" : "stopped");
}

void ObdCalibration::setNissanProbesEnabled(bool enabled) {
    setNissanProbesEnabled(enabled, _lastNow);
}

void ObdCalibration::setNissanProbesEnabled(bool enabled, uint32_t now) {
    _lastNow = now;
    if (_nissanProbesEnabled == enabled) return;
    if (!enabled) cancelNissan(now, "profile_changed");
    _nissanProbesEnabled = enabled;
    // A profile toggle does not restart groups already attempted during LOG ON.
}

void ObdCalibration::cancelNissan(uint32_t now, const char* reason) {
    if (_nissanStep == NissanStep::Idle && !_sessionMayBeOpen) return;
    const bool mayBeOpen = _sessionMayBeOpen;
    _pending = false;
    const bool canClose = mayBeOpen && uint32_t(now - _lastAttempt) >= REQUEST_INTERVAL_MS;
    if (canClose) requestSession(0x81, now);
    log("ODB SESSION_STATE t_ms=%lu status=cancelled reason=%s state=%s close=%s",
        static_cast<unsigned long>(now), reason, mayBeOpen ? "unknown" : "not_opened",
        canClose ? (_pending ? "queued_unconfirmed" : "tx_failed") :
                   (mayBeOpen ? "request_interval" : "not_needed"));
    _pending = false;
    _isProbe = _isSession = false;
    _sessionMayBeOpen = false;
    _nissanStep = NissanStep::Idle;
    _nissanBlocked = true;
}

bool ObdCalibration::supports(uint8_t ecu, uint8_t pid) const {
    if (pid == 0 || pid > 0x60) return false;
    const uint8_t page = (pid - 1) / 32;
    const uint8_t bit = 32 - (pid - page * 32);
    return _ecus[ecu].valid[page] && (_ecus[ecu].support[page] & (uint32_t(1) << bit));
}

bool ObdCalibration::anySupports(uint8_t pid) const {
    for (uint8_t ecu = 0; ecu < 8; ++ecu) if (supports(ecu, pid)) return true;
    return false;
}

void ObdCalibration::request(uint8_t pid, uint16_t id, uint32_t now) {
    CanFrame frame = {};
    frame.identifier = id;
    frame.data_length_code = 8;
    frame.data[0] = 2;
    frame.data[1] = 1;
    frame.data[2] = pid;
    _hasAttempt = true;
    _lastAttempt = now;
    ++_sequence;
    const bool queued = _send && _send(frame, _context);
    log("ODB TX t_ms=%lu seq=%lu can_id=0x%03X service=0x01 pid=0x%02X status=%s data=02 01 %02X 00 00 00 00 00",
        static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence), id, pid,
        queued ? "queued" : "failed", pid);
    if (!queued) return;
    _isProbe = _isSession = false;
    _pid = pid;
    _requestId = id;
    _sentAt = now;
    _responses = _positives = _negatives = 0;
    _pending = true;
}

void ObdCalibration::finish(uint32_t now) {
    if (_isProbe || _isSession) {
        finishNissan(now);
        return;
    }
    if (!_responses) {
        log("ODB TIMEOUT t_ms=%lu seq=%lu request_id=0x%03X pid=0x%02X elapsed_ms=%lu",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            _requestId, _pid, static_cast<unsigned long>(uint32_t(now - _sentAt)));
    }
    _pending = false;
    if (_stage == Stage::Page00) {
        if (!_positives) {
            if (++_attempts >= 3) {
                _stage = Stage::Retry;
                _retryAt = now;
                log("ODB STATE t_ms=%lu status=no_supported_response retry_ms=%lu",
                    static_cast<unsigned long>(now), static_cast<unsigned long>(DISCOVERY_RETRY_MS));
            }
            return;
        }
        _stage = anySupports(0x20) ? Stage::Page20 : Stage::Poll;
    } else if (_stage == Stage::Page20) {
        _stage = anySupports(0x40) ? Stage::Page40 : Stage::Poll;
    } else if (_stage == Stage::Page40) {
        _stage = Stage::Poll;
    }
    if (_stage == Stage::Poll) {
        bool available = false;
        for (const auto& entry : PARAMETERS) available |= anySupports(entry.pid);
        if (!available) {
            _stage = Stage::Retry;
            _retryAt = now;
            log("ODB STATE t_ms=%lu status=no_supported_reference_pids retry_ms=%lu",
                static_cast<unsigned long>(now), static_cast<unsigned long>(DISCOVERY_RETRY_MS));
        }
    }
}

void ObdCalibration::tick(uint32_t now) {
    _lastNow = now;
    if (!_enabled) return;
    if (_pending) {
        if (uint32_t(now - _sentAt) < RESPONSE_WINDOW_MS) return;
        finish(now);
    }
    if (_nissanStep != NissanStep::Idle) {
        runNissan(now);
        return;
    }
    if (_stage == Stage::Retry) {
        if (uint32_t(now - _retryAt) < DISCOVERY_RETRY_MS) return;
        _stage = Stage::Page00;
        _attempts = 0;
        for (auto& ecu : _ecus) ecu = Ecu{};
    }
    if (_hasAttempt && uint32_t(now - _lastAttempt) < REQUEST_INTERVAL_MS) return;
    switch (_stage) {
        case Stage::Page00: request(0x00, 0x7DF, now); return;
        case Stage::Page20: request(0x20, 0x7DF, now); return;
        case Stage::Page40: request(0x40, 0x7DF, now); return;
        default: break;
    }
    bool fastAvailable = false;
    for (const auto pid : FAST_PIDS) fastAvailable |= anySupports(pid);
    if (!fastAvailable || _scheduleSlot == 2) {
        _scheduleSlot = 0;
        if (pollProbe(now) || poll(false, fastAvailable, now)) return;
    } else {
        ++_scheduleSlot;
        if (poll(true, false, now)) return;
    }
    // A vehicle with only fast PIDs, or no fast PIDs, still keeps polling.
    if (!poll(true, false, now)) poll(false, false, now);
}

bool ObdCalibration::poll(bool fast, bool skipFast, uint32_t now) {
    const uint8_t count = fast ? FAST_COUNT : PARAMETER_COUNT;
    for (uint8_t n = 0; n < count; ++n) {
        const uint8_t pid = fast ? FAST_PIDS[_fastIndex] : PARAMETERS[_pollIndex].pid;
        if (fast) _fastIndex = (_fastIndex + 1) % FAST_COUNT;
        else _pollIndex = (_pollIndex + 1) % PARAMETER_COUNT;
        if (skipFast && isFast(pid)) continue;
        for (uint8_t ecu = 0; ecu < 8; ++ecu) {
            if (supports(ecu, pid)) {
                request(pid, 0x7E0 + ecu, now);
                return true;
            }
        }
    }
    return false;
}

bool ObdCalibration::pollProbe(uint32_t now) {
    if (!_nissanProbesEnabled || _nissanBlocked || !_ecus[0].valid[0] ||
        (_completedGroups && uint32_t(now - _lastGroupAttempt) < GROUP_INTERVAL_MS)) return false;
    for (uint8_t index = 0; index < GROUP_COUNT; ++index) {
        if (_completedGroups & (uint8_t(1) << index)) continue;
        _nissanGroup = index;
        _completedGroups |= uint8_t(1) << index;
        _lastGroupAttempt = _groupStarted = now;
        _nissanStep = NissanStep::Open;
        requestSession(0xC0, now);
        return true;
    }
    return false;
}

void ObdCalibration::requestSession(uint8_t command, uint32_t now) {
    CanFrame frame = {};
    frame.identifier = 0x7E0;
    frame.data_length_code = 8;
    frame.data[0] = 2; frame.data[1] = 0x10; frame.data[2] = command;
    _hasAttempt = true;
    _lastAttempt = now;
    ++_sequence;
    const bool queued = _send && _send(frame, _context);
    log("ODB TX t_ms=%lu seq=%lu can_id=0x7E0 service=0x10 subfunction=0x%02X status=%s data=02 10 %02X 00 00 00 00 00",
        static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence), command,
        queued ? "queued" : "failed", command);
    _isSession = true; _isProbe = false;
    _sessionCommand = command;
    _requestId = 0x7E0;
    _sentAt = now;
    _responses = _positives = _negatives = 0;
    _probeTerminalNegative = false; _probeLastNrc = 0;
    _pending = queued;
    if (queued && command == 0xC0) _sessionMayBeOpen = true;
    if (!queued) {
        if (command == 0x81) {
            _nissanBlocked = true;
            log("ODB SESSION_STATE t_ms=%lu status=close_failed state=unknown",
                static_cast<unsigned long>(now));
        }
        _sessionMayBeOpen = false;
        _nissanStep = NissanStep::Idle;
        _isSession = false;
    }
}

void ObdCalibration::requestProbe(uint8_t index, uint32_t now) {
    const auto& probe = NISSAN_PROBES[index];
    CanFrame frame = {};
    frame.identifier = 0x7E0;
    frame.data_length_code = 8;
    frame.data[0] = 3; frame.data[1] = 0x22;
    frame.data[2] = probe.did >> 8; frame.data[3] = probe.did;
    _hasAttempt = true;
    _lastAttempt = now;
    ++_sequence;
    const bool queued = _send && _send(frame, _context);
    log("ODB TX t_ms=%lu seq=%lu can_id=0x7E0 service=0x22 did=0x%04X status=%s data=03 22 %02X %02X 00 00 00 00",
        static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence), probe.did,
        queued ? "queued" : "failed", frame.data[2], frame.data[3]);
    _isProbe = true; _isSession = false; _activeProbe = index;
    _requestId = 0x7E0; _sentAt = now;
    _responses = _positives = _negatives = 0;
    _probeTerminalNegative = false; _probeLastNrc = 0;
    _pending = queued;
    if (!queued) { _isProbe = false; _nissanStep = NissanStep::Close; }
}

void ObdCalibration::finishNissan(uint32_t now) {
    if (!_positives && !_probeTerminalNegative) {
        if (_isSession) log("ODB TIMEOUT t_ms=%lu seq=%lu request_id=0x7E0 service=0x10 subfunction=0x%02X elapsed_ms=%lu",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence), _sessionCommand,
            static_cast<unsigned long>(uint32_t(now - _sentAt)));
        else log("ODB TIMEOUT t_ms=%lu seq=%lu request_id=0x7E0 service=0x22 did=0x%04X elapsed_ms=%lu",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            NISSAN_PROBES[_activeProbe].did, static_cast<unsigned long>(uint32_t(now - _sentAt)));
    }
    _pending = false;
    if (_isSession) {
        const bool accepted = _positives && !_probeTerminalNegative;
        if (_sessionCommand == 0xC0) {
            _nissanStep = accepted ? NissanStep::First : NissanStep::Close;
            if (!accepted) _nissanBlocked = true;
        } else {
            _nissanStep = NissanStep::Idle;
            _sessionMayBeOpen = false;
            if (!accepted) {
                _nissanBlocked = true;
                log("ODB SESSION_STATE t_ms=%lu status=close_unconfirmed state=unknown",
                    static_cast<unsigned long>(now));
            }
        }
    } else {
        _nissanStep = _nissanStep == NissanStep::First && NISSAN_GROUPS[_nissanGroup].count == 2
            ? NissanStep::Second : NissanStep::Close;
    }
    _isSession = _isProbe = false;
}

void ObdCalibration::runNissan(uint32_t now) {
    if (_hasAttempt && uint32_t(now - _lastAttempt) < REQUEST_INTERVAL_MS) return;
    if (_nissanStep != NissanStep::Close && uint32_t(now - _groupStarted) >= GROUP_READ_LIMIT_MS) {
        _nissanStep = NissanStep::Close;
        log("ODB SESSION_STATE t_ms=%lu status=read_deadline state=unknown",
            static_cast<unsigned long>(now));
    }
    const auto& group = NISSAN_GROUPS[_nissanGroup];
    switch (_nissanStep) {
        case NissanStep::First: requestProbe(group.first, now); break;
        case NissanStep::Second: requestProbe(group.first + 1, now); break;
        case NissanStep::Close: requestSession(0x81, now); break;
        default: break;
    }
}

void ObdCalibration::receive(const CanFrame& frame, uint32_t now) {
    _lastNow = now;
    if (!_enabled || !_pending || uint32_t(now - _sentAt) >= RESPONSE_WINDOW_MS ||
        frame.extd || frame.rtr || frame.data_length_code != 8 ||
        frame.identifier < 0x7E8 || frame.identifier > 0x7EF ||
        (_requestId != 0x7DF && frame.identifier != uint32_t(_requestId + 8)) ||
        (frame.data[0] & 0xF0) != 0) return;
    const uint8_t length = frame.data[0] & 0x0F;
    const uint8_t ecu = frame.identifier - 0x7E8;
    const uint8_t mask = uint8_t(1) << ecu;
    if (length == 3 && frame.data[1] == 0x7F && frame.data[2] == (_isSession ? 0x10 : _isProbe ? 0x22 : 0x01)) {
        if (_negatives & mask) {
            if ((!_isProbe && !_isSession) || _probeLastNrc == frame.data[3]) return;
        }
        _negatives |= mask;
        _responses |= mask;
        if (_isProbe || _isSession) {
            _probeLastNrc = frame.data[3];
            if (frame.data[3] != 0x78) _probeTerminalNegative = true;
            if (_isSession) log("ODB NEGATIVE t_ms=%lu seq=%lu request_id=0x7E0 ecu=0x%03lX service=0x10 subfunction=0x%02X nrc=0x%02X",
                static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
                static_cast<unsigned long>(frame.identifier), _sessionCommand, frame.data[3]);
            else log("ODB NEGATIVE t_ms=%lu seq=%lu request_id=0x%03X ecu=0x%03lX service=0x22 did=0x%04X nrc=0x%02X",
                static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
                _requestId, static_cast<unsigned long>(frame.identifier), NISSAN_PROBES[_activeProbe].did,
                frame.data[3]);
        } else log("ODB NEGATIVE t_ms=%lu seq=%lu request_id=0x%03X ecu=0x%03lX pid=0x%02X nrc=0x%02X",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            _requestId, static_cast<unsigned long>(frame.identifier), _pid, frame.data[3]);
        return;
    }
    if (_isSession) {
        if (_probeTerminalNegative || length < 2 || length > 7 || frame.data[1] != 0x50 ||
            frame.data[2] != _sessionCommand || (_positives & mask)) return;
        log("ODB SESSION_RX t_ms=%lu seq=%lu request_id=0x7E0 ecu=0x%03lX service=0x10 subfunction=0x%02X status=%s length=%u latency_ms=%lu",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            static_cast<unsigned long>(frame.identifier), _sessionCommand,
            _sessionCommand == 0xC0 ? "opened" : "closed", unsigned(length),
            static_cast<unsigned long>(uint32_t(now - _sentAt)));
        _positives |= mask; _responses |= mask;
        return;
    }
    if (_isProbe) {
        const auto& probe = NISSAN_PROBES[_activeProbe];
        if (_probeTerminalNegative || length != 4 || frame.data[1] != 0x62 ||
            frame.data[2] != (probe.did >> 8) || frame.data[3] != (probe.did & 0xFF) ||
            (_positives & mask)) return;
        const uint8_t raw = frame.data[4];
        const bool temperature = probe.did == 0x111F || probe.did == 0x1101;
        log("ODB PROBE_RX t_ms=%lu seq=%lu request_id=0x%03X ecu=0x%03lX service=0x22 did=0x%04X name=%s raw=0x%02X value=%.3f unit=%s latency_ms=%lu",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            _requestId, static_cast<unsigned long>(frame.identifier), probe.did, probe.name,
            raw, temperature ? double(int(raw) - 50) : double(raw), temperature ? "degC" : "raw",
            static_cast<unsigned long>(uint32_t(now - _sentAt)));
        _positives |= mask;
        _responses |= mask;
        return;
    }
    if (frame.data[1] != 0x41 || frame.data[2] != _pid || (_positives & mask)) return;
    const bool discovery = _pid == 0x00 || _pid == 0x20 || _pid == 0x40;
    const Parameter* entry = parameter(_pid);
    if (length != (discovery ? 6 : entry ? 2 + entry->bytes : 0)) return;
    if (discovery) {
        const uint8_t page = _pid / 32;
        if (page && !supports(ecu, _pid)) return;
        uint32_t bitmap = 0;
        for (uint8_t i = 3; i < 7; ++i) bitmap = (bitmap << 8) | frame.data[i];
        _ecus[ecu].support[page] = bitmap;
        _ecus[ecu].valid[page] = true;
        log("ODB SUPPORTED t_ms=%lu seq=%lu request_id=0x%03X ecu=0x%03lX pid=0x%02X bitmap=0x%08lX",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            _requestId, static_cast<unsigned long>(frame.identifier), _pid,
            static_cast<unsigned long>(bitmap));
    } else if (entry) {
        const uint16_t raw = entry->bytes >= 2 ? uint16_t(frame.data[3]) * 256 + frame.data[4]
                                               : frame.data[3];
        double value = raw;
        switch (_pid) {
            case 0x0C: value = raw / 4.0; break;
            case 0x05: case 0x46: case 0x0F: case 0x5C: value = int(raw) - 40; break;
            case 0x2F: case 0x04: case 0x11: value = raw * 100.0 / 255; break;
            case 0x06: case 0x07: value = (int(raw) - 128) * 100.0 / 128; break;
            case 0x03: value = frame.data[3]; break;
            case 0x10: value = raw / 100.0; break;
            case 0x24: case 0x44: value = raw / 32768.0; break;
            case 0x42: value = raw / 1000.0; break;
            case 0x5E: value = raw / 20.0; break;
            default: break;
        }
        log("ODB RX t_ms=%lu seq=%lu request_id=0x%03X ecu=0x%03lX pid=0x%02X name=%s value=%.3f unit=%s latency_ms=%lu",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            _requestId, static_cast<unsigned long>(frame.identifier), _pid,
            entry->name, value, entry->unit, static_cast<unsigned long>(uint32_t(now - _sentAt)));
        if (_pid == 0x03 || _pid == 0x24) {
            const bool status = _pid == 0x03;
            const double extra = status ? double(frame.data[4]) :
                (uint16_t(frame.data[5]) * 256 + frame.data[6]) / 8192.0;
            log("ODB RX t_ms=%lu seq=%lu request_id=0x%03X ecu=0x%03lX pid=0x%02X name=%s value=%.3f unit=%s latency_ms=%lu",
                static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
                _requestId, static_cast<unsigned long>(frame.identifier), _pid,
                status ? "fuel_system_2" : "wideband_o2_voltage_1", extra,
                status ? "bitmask" : "V", static_cast<unsigned long>(uint32_t(now - _sentAt)));
        }
    } else return;
    _positives |= mask;
    _responses |= mask;
}
