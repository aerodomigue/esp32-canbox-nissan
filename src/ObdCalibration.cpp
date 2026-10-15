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
};
constexpr uint8_t PARAMETER_COUNT = sizeof(PARAMETERS) / sizeof(PARAMETERS[0]);
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
    if (_enabled == enabled) return;
    _enabled = enabled;
    _pending = false;
    _stage = Stage::Page00;
    _attempts = _pollIndex = 0;
    _lastAttempt = now - REQUEST_INTERVAL_MS;
    for (auto& ecu : _ecus) ecu = Ecu{};
    log("ODB STATE t_ms=%lu enabled=%u status=%s", static_cast<unsigned long>(now),
        enabled ? 1U : 0U, enabled ? "discovery" : "stopped");
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
    _lastAttempt = now;
    ++_sequence;
    const bool queued = _send && _send(frame, _context);
    log("ODB TX t_ms=%lu seq=%lu can_id=0x%03X service=0x01 pid=0x%02X status=%s data=02 01 %02X 00 00 00 00 00",
        static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence), id, pid,
        queued ? "queued" : "failed", pid);
    if (!queued) return;
    _pid = pid;
    _requestId = id;
    _sentAt = now;
    _responses = _positives = _negatives = 0;
    _pending = true;
}

void ObdCalibration::finish(uint32_t now) {
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
    if (!_enabled) return;
    if (_pending) {
        if (uint32_t(now - _sentAt) < RESPONSE_WINDOW_MS) return;
        finish(now);
    }
    if (_stage == Stage::Retry) {
        if (uint32_t(now - _retryAt) < DISCOVERY_RETRY_MS) return;
        _stage = Stage::Page00;
        _attempts = 0;
        for (auto& ecu : _ecus) ecu = Ecu{};
    }
    if (uint32_t(now - _lastAttempt) < REQUEST_INTERVAL_MS) return;
    switch (_stage) {
        case Stage::Page00: request(0x00, 0x7DF, now); return;
        case Stage::Page20: request(0x20, 0x7DF, now); return;
        case Stage::Page40: request(0x40, 0x7DF, now); return;
        default: break;
    }
    for (uint8_t n = 0; n < PARAMETER_COUNT; ++n) {
        const uint8_t pid = PARAMETERS[_pollIndex].pid;
        _pollIndex = (_pollIndex + 1) % PARAMETER_COUNT;
        for (uint8_t ecu = 0; ecu < 8; ++ecu) {
            if (supports(ecu, pid)) {
                request(pid, 0x7E0 + ecu, now);
                return;
            }
        }
    }
}

void ObdCalibration::receive(const CanFrame& frame, uint32_t now) {
    if (!_enabled || !_pending || uint32_t(now - _sentAt) >= RESPONSE_WINDOW_MS ||
        frame.extd || frame.rtr || frame.data_length_code != 8 ||
        frame.identifier < 0x7E8 || frame.identifier > 0x7EF ||
        (_requestId != 0x7DF && frame.identifier != uint32_t(_requestId + 8)) ||
        (frame.data[0] & 0xF0) != 0) return;
    const uint8_t length = frame.data[0] & 0x0F;
    const uint8_t ecu = frame.identifier - 0x7E8;
    const uint8_t mask = uint8_t(1) << ecu;
    if (length == 3 && frame.data[1] == 0x7F && frame.data[2] == 0x01) {
        if (_negatives & mask) return;
        _negatives |= mask;
        _responses |= mask;
        log("ODB NEGATIVE t_ms=%lu seq=%lu request_id=0x%03X ecu=0x%03lX pid=0x%02X nrc=0x%02X",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            _requestId, static_cast<unsigned long>(frame.identifier), _pid, frame.data[3]);
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
        const uint16_t raw = entry->bytes == 2 ? uint16_t(frame.data[3]) * 256 + frame.data[4]
                                               : frame.data[3];
        double value = raw;
        switch (_pid) {
            case 0x0C: value = raw / 4.0; break;
            case 0x05: case 0x46: value = int(raw) - 40; break;
            case 0x2F: value = raw * 100.0 / 255; break;
            case 0x42: value = raw / 1000.0; break;
            case 0x5E: value = raw / 20.0; break;
            default: break;
        }
        log("ODB RX t_ms=%lu seq=%lu request_id=0x%03X ecu=0x%03lX pid=0x%02X name=%s value=%.3f unit=%s latency_ms=%lu",
            static_cast<unsigned long>(now), static_cast<unsigned long>(_sequence),
            _requestId, static_cast<unsigned long>(frame.identifier), _pid,
            entry->name, value, entry->unit, static_cast<unsigned long>(uint32_t(now - _sentAt)));
    } else return;
    _positives |= mask;
    _responses |= mask;
}
