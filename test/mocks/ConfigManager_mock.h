#pragma once
#include <stdint.h>
#include <string.h>

// =============================================================================
// ConfigManager mock state — used by tests for assertions and setup
// Actual function implementations are in ConfigManager_stub.cpp
// =============================================================================

struct MockConfigState {
    uint16_t steerScale       = 0;
    int16_t  steerOffset      = 0;
    bool     steerInvert      = false;
    uint16_t indicatorTimeout = 0;
    uint8_t  rpmDivisor       = 0;
    uint8_t  tankCapacity     = 0;
    uint16_t dteDivisor       = 0;

    int resetCount = 0;
    int saveCount  = 0;

    char vehicleFile[64] = "";

    // Tracks which setters were called
    bool steerScaleSet       = false;
    bool steerOffsetSet      = false;
    bool steerInvertSet      = false;
    bool indicatorTimeoutSet = false;
    bool rpmDivisorSet       = false;
    bool tankCapacitySet     = false;
    bool dteDivisorSet       = false;
};

extern MockConfigState g_mock;

inline void mockReset() {
    g_mock = MockConfigState{};
}
