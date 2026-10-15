#ifndef FUEL_CONSUMPTION_ESTIMATOR_H
#define FUEL_CONSUMPTION_ESTIMATOR_H

#include <cmath>
#include <cstdint>

// This coefficient is a configurable hypothesis, not a Juke calibration.
struct FuelConsumptionConfig {
    bool enabled = false;
    double mm3PerTick = 80.0;
    uint32_t windowMs = 1000;
    uint32_t maxGapMs = 500;
    double minSpeedKmh = 2.0;
    double minAverageDistanceM = 100.0;
    double maxTicksPerSecond = 400.0;

    bool isValid() const {
        return std::isfinite(mm3PerTick) && mm3PerTick > 0.0 &&
               windowMs >= 100 && windowMs <= 10000 &&
               maxGapMs >= 100 && maxGapMs <= 1000 &&
               std::isfinite(minSpeedKmh) && minSpeedKmh >= 0.0 &&
               std::isfinite(minAverageDistanceM) && minAverageDistanceM > 0.0 &&
               std::isfinite(maxTicksPerSecond) && maxTicksPerSecond > 0.0 &&
               maxTicksPerSecond * (maxGapMs / 1000.0) + 1.0 < 256.0;
    }
};

/**
 * Convert an unsigned modulo-256 fuel counter into 0.1 L/100km radio units.
 *
 * Both speed and counter must remain fresh. A discontinuity drops the entire
 * session: partial fuel must never be divided by uninterrupted trip distance.
 * Sessions start on a counter sample with a fresh speed, so the initial raw
 * count is a baseline, never an already-consumed volume.
 *
 * Speed is held between observations; pending distance is committed only on
 * accepted counter samples. Thus fuel and distance always cover the same
 * interval. Average consumption includes fuel used while stationary.
 *
 * The plausibility budget permits queued frames with identical software
 * timestamps. It is bounded below one complete counter wrap. Some ECU resets
 * still look exactly like normal increments and cannot be detected from this
 * byte alone. The configured maximum rate is an assumption, not a sensor.
 */
class FuelConsumptionEstimator {
public:
    bool configure(const FuelConsumptionConfig& config) {
        _config = config;
        const bool valid = config.isValid();
        if (!valid) _config.enabled = false;
        clearState();
        return valid;
    }

    void onSpeed(double kmh, uint32_t now) {
        if (!_config.enabled) return;
        tick(now);
        if (!std::isfinite(kmh) || kmh < 0.0) {
            clearState();
            return;
        }
        if (_running && !advanceDistance(now)) clearState();
        _speedKmh = kmh;
        _speedAt = now;
        _speedSeen = true;
        if (_running && belowDisplaySpeed()) {
            _instantaneous = 0;
            _instantaneousValid = true;
        }
    }

    void onCounter(uint8_t counter, uint32_t now) {
        if (!_config.enabled) return;
        tick(now);
        if (!_running) {
            seedCounter(counter, now);
            if (_speedSeen) startSession(now);
            return;
        }

        // Unsigned narrowing handles 255 -> 0 as one increment.
        const uint8_t delta = static_cast<uint8_t>(counter - _counter);
        const uint32_t elapsed = now - _budgetAt;
        const double cap = _config.maxTicksPerSecond *
                           (_config.maxGapMs / 1000.0) + 1.0;
        const double replenished = _tickBudget +
            _config.maxTicksPerSecond * (elapsed / 1000.0);
        _tickBudget = replenished < cap ? replenished : cap;
        _budgetAt = now;
        if (static_cast<double>(delta) > _tickBudget + 1e-9 ||
            !advanceDistance(now)) {
            rebaseCounter(counter, now);
            return;
        }

        const double volume = delta * _config.mm3PerTick;
        if (!std::isfinite(volume) ||
            !std::isfinite(_totalFuelMm3 + volume) ||
            !std::isfinite(_totalDistanceM + _pendingDistanceM) ||
            !std::isfinite(_windowFuelMm3 + volume) ||
            !std::isfinite(_windowDistanceM + _pendingDistanceM)) {
            rebaseCounter(counter, now);
            return;
        }
        _tickBudget -= delta;
        _totalFuelMm3 += volume;
        _windowFuelMm3 += volume;
        _totalDistanceM += _pendingDistanceM;
        _windowDistanceM += _pendingDistanceM;
        _pendingDistanceM = 0.0;
        seedCounter(counter, now);

        if (_totalDistanceM >= _config.minAverageDistanceM) {
            _average = radioUnits(_totalFuelMm3, _totalDistanceM);
            _averageValid = true;
        }

        const uint32_t windowElapsed = now - _windowAt;
        if (windowElapsed >= _config.windowMs) {
            const double meanSpeedKmh = _windowDistanceM *
                                       (3600.0 / windowElapsed);
            // Zero below the speed threshold is a display convention, not
            // a claim that the engine consumes no fuel while stationary.
            _instantaneous = belowDisplaySpeed() ||
                meanSpeedKmh < _config.minSpeedKmh || _windowDistanceM <= 0.0
                ? 0 : radioUnits(_windowFuelMm3, _windowDistanceM);
            _instantaneousValid = true;
            _windowAt = now;
            _windowFuelMm3 = 0.0;
            _windowDistanceM = 0.0;
        }
    }

    // Call even when the CAN bus is silent, before publishing dashboard data.
    void tick(uint32_t now) {
        if (!_config.enabled) return;
        if ((_speedSeen && uint32_t(now - _speedAt) > _config.maxGapMs) ||
            (_counterSeen && uint32_t(now - _counterAt) > _config.maxGapMs)) {
            clearState();
        }
    }

    bool instantaneousValid() const { return _instantaneousValid; }
    bool averageValid() const { return _averageValid; }
    uint16_t instantaneous() const { return _instantaneous; }
    uint16_t average() const { return _average; }
    double totalFuelMm3() const { return _totalFuelMm3; }
    double totalDistanceM() const { return _totalDistanceM; }

private:
    FuelConsumptionConfig _config;
    bool _speedSeen = false;
    bool _counterSeen = false;
    bool _running = false;
    bool _instantaneousValid = false;
    bool _averageValid = false;
    uint8_t _counter = 0;
    uint16_t _instantaneous = 0;
    uint16_t _average = 0;
    uint32_t _speedAt = 0;
    uint32_t _counterAt = 0;
    uint32_t _integratedAt = 0;
    uint32_t _windowAt = 0;
    uint32_t _budgetAt = 0;
    double _speedKmh = 0.0;
    double _tickBudget = 1.0;
    double _pendingDistanceM = 0.0;
    double _totalFuelMm3 = 0.0;
    double _totalDistanceM = 0.0;
    double _windowFuelMm3 = 0.0;
    double _windowDistanceM = 0.0;

    void clearState() {
        _speedSeen = _counterSeen = _running = false;
        _instantaneousValid = _averageValid = false;
        _counter = 0;
        _instantaneous = _average = 0;
        _speedAt = _counterAt = _integratedAt = _windowAt = _budgetAt = 0;
        _speedKmh = _pendingDistanceM = 0.0;
        _totalFuelMm3 = _totalDistanceM = 0.0;
        _windowFuelMm3 = _windowDistanceM = 0.0;
        _tickBudget = 1.0;
    }

    void seedCounter(uint8_t counter, uint32_t now) {
        _counter = counter;
        _counterAt = now;
        _counterSeen = true;
    }

    void rebaseCounter(uint8_t counter, uint32_t now) {
        clearState();
        seedCounter(counter, now);
    }

    void startSession(uint32_t now) {
        _running = true;
        _integratedAt = _windowAt = _budgetAt = now;
        if (belowDisplaySpeed()) {
            _instantaneous = 0;
            _instantaneousValid = true;
        }
    }

    bool belowDisplaySpeed() const {
        return _speedKmh <= 0.0 || _speedKmh < _config.minSpeedKmh;
    }

    bool advanceDistance(uint32_t now) {
        const double next = _pendingDistanceM +
                            _speedKmh * (uint32_t(now - _integratedAt) / 3600.0);
        if (!std::isfinite(next)) return false;
        _pendingDistanceM = next;
        _integratedAt = now;
        return true;
    }

    static uint16_t radioUnits(double fuelMm3, double distanceM) {
        // mm3 / m equals 0.1 L/100km. Reserve 0xFFFF (unavailable average).
        const double units = fuelMm3 / distanceM;
        if (!std::isfinite(units) || units >= 65534.0) return 65534;
        return static_cast<uint16_t>(units + 0.5);
    }
};

#endif
