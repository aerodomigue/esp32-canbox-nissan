#ifndef BOOT_TRIP_COMPUTER_H
#define BOOT_TRIP_COMPUTER_H

#include <cmath>
#include <cstdint>
#include <limits>

/**
 * RAM-only trip distance and mean speed for one device boot.
 *
 * Uptime includes setup and stationary periods. Speed is held between fresh
 * observations, for at most 500 ms. A longer gap or an invalid observation
 * keeps distance already integrated, but permanently marks the trip incomplete;
 * later samples cannot reconstruct the unobserved distance. A new object is the
 * only reset. Call tick() even when CAN is silent so a gap is detected promptly.
 */
class BootTripComputer {
public:
    void tick(uint32_t now) {
        if (!_clockSeen) {
            // millis() is measured from boot, not from the first CAN sample.
            _uptimeMs = now;
            _clockSeen = true;
        } else {
            _uptimeMs += static_cast<uint32_t>(now - _clockAt);
        }
        _clockAt = now;

        if (!_tracking) return;
        if (static_cast<uint32_t>(now - _speedAt) > MAX_GAP_MS) {
            markDiscontinuity();
            return;
        }
        _distanceM += _speedKmh *
                      (static_cast<uint32_t>(now - _integratedAt) / 3600.0);
        _integratedAt = now;
    }

    void onSpeed(double kmh, uint32_t now) {
        tick(now);
        if (!std::isfinite(kmh) || kmh < 0.0 || kmh >= 300.0) {
            markDiscontinuity();
            return;
        }
        _hasStarted = true;
        _tracking = true;
        _speedKmh = kmh;
        _speedAt = _integratedAt = now;
    }

    void markDiscontinuity() {
        if (_hasStarted) _complete = false;
        _tracking = false;
    }

    bool valid() const { return _hasStarted && _complete; }
    bool hasStarted() const { return _hasStarted; }
    bool complete() const { return _complete; }
    double distanceM() const { return _distanceM; }

    uint32_t elapsedSeconds() const {
        const uint64_t seconds = _uptimeMs / 1000u;
        return seconds >= std::numeric_limits<uint32_t>::max()
            ? std::numeric_limits<uint32_t>::max()
            : static_cast<uint32_t>(seconds);
    }

    uint16_t averageSpeedDeciKmh() const {
        if (_uptimeMs == 0) return 0;
        // m / ms * 3600 = km/h; the radio uses tenths of km/h.
        const double value = _distanceM * 36000.0 / _uptimeMs;
        return value >= std::numeric_limits<uint16_t>::max()
            ? std::numeric_limits<uint16_t>::max()
            : static_cast<uint16_t>(value + 0.5);
    }

    uint32_t distanceDeciKm() const {
        // Completed 100 m increments, without rounding up a partial increment.
        const double value = _distanceM / 100.0;
        return value >= std::numeric_limits<uint32_t>::max()
            ? std::numeric_limits<uint32_t>::max()
            : static_cast<uint32_t>(value);
    }

private:
    static constexpr uint32_t MAX_GAP_MS = 500;
    bool _clockSeen = false;
    bool _hasStarted = false;
    bool _complete = true;
    bool _tracking = false;
    uint32_t _clockAt = 0;
    uint32_t _speedAt = 0;
    uint32_t _integratedAt = 0;
    uint64_t _uptimeMs = 0;
    double _speedKmh = 0.0;
    double _distanceM = 0.0;
};

#endif
