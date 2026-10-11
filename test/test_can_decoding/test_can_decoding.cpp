/**
 * @file test_can_decoding.cpp
 * @brief Regression tests for CAN frame → GlobalData decoding pipeline
 *
 * Loads production config (data/NissanJukeF15.json) via LittleFS mock,
 * feeds raw CAN frames into processFrame(), and asserts GlobalData values.
 * Covers the original active preset and separate, deferred PR review mappings.
 *
 * Run: pio test -e native
 */

#include <unity.h>
#include "CanConfigProcessor.h"
#include "ConfigManager_mock.h"
#include "LittleFS.h"

// GlobalData externs (defined in GlobalData_stub.cpp)
extern int16_t  currentSteer;
extern uint16_t engineRPM;
extern uint8_t  vehicleSpeed;
extern uint8_t  currentDoors;
extern uint8_t  fuelLevel;
extern float    voltBat;
extern int16_t  dteValue;
extern int8_t   tempExt;
extern int8_t   coolantTemp;
extern uint32_t currentOdo;
extern bool     headlightsOn;
extern bool     highBeamOn;
extern bool     parkingLightsOn;
extern uint16_t fuelConsumptionInst;
extern uint16_t fuelConsumptionAvg;
extern unsigned long lastLeftIndicatorTime;
extern unsigned long lastRightIndicatorTime;

static CanConfigProcessor proc;

// Reset all GlobalData to 0/false before each test
static void resetGlobalData() {
    currentSteer = 0; engineRPM = 0; vehicleSpeed = 0;
    currentDoors = 0; fuelLevel = 0; voltBat = 0.0f;
    dteValue = 0; tempExt = 0; coolantTemp = 0; currentOdo = 0;
    headlightsOn = false; highBeamOn = false; parkingLightsOn = false;
    fuelConsumptionInst = 0; fuelConsumptionAvg = 0;
    lastLeftIndicatorTime = 0; lastRightIndicatorTime = 0;
}

static CanFrame makeFrame(uint32_t id, const uint8_t* bytes, uint8_t len) {
    CanFrame f = {};
    f.identifier = id;
    f.data_length_code = len;
    for (uint8_t i = 0; i < len && i < 8; i++) f.data[i] = bytes[i];
    return f;
}

void setUp() {
    mockReset();
    LittleFS.basePath = "data";  // use production config file
    resetGlobalData();
    TEST_ASSERT_TRUE(proc.loadFromJson("/NissanJukeF15.json"));
}

void tearDown() {}

// =============================================================================
// TEMPERATURE EXTERNAL — 0x540 byte0, raw-40
// Validated: dashboard showed 28°C, raw=0x44=68, 68-40=28
// =============================================================================

void test_temp_ext_28c() {
    uint8_t d[] = {0x44, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x540, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT8(28, tempExt);
}

void test_temp_ext_zero() {
    uint8_t d[] = {0x28, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x540, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT8(0, tempExt);  // 0x28=40, 40-40=0
}

void test_temp_ext_negative() {
    uint8_t d[] = {0x1E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x540, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT8(-10, tempExt);  // 0x1E=30, 30-40=-10
}

// =============================================================================
// COOLANT TEMPERATURE — 0x551 byte0, raw-40
// Validated: dashboard at ~1/4 gauge, raw=0x5A=90, 90-40=50°C
// =============================================================================

void test_coolant_50c() {
    uint8_t d[] = {0x5A, 0xC0, 0x00, 0x80, 0xFF, 0x02, 0x80, 0xFF};
    CanFrame f = makeFrame(0x551, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT8(50, coolantTemp);
}

void test_coolant_operating_temp() {
    uint8_t d[] = {0x82, 0xC0, 0x00, 0x80, 0xFF, 0x02, 0x80, 0xFF};
    CanFrame f = makeFrame(0x551, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT8(90, coolantTemp);  // 0x82=130, 130-40=90°C (operating)
}

// =============================================================================
// DISTANCE TO EMPTY — active 0x54C bytes 6-7 BE, raw*100/338
// =============================================================================

void test_dte_305km() {
    // 1032*100/338 truncates to 305; other byte pairs deliberately disagree.
    uint8_t d[] = {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC, 0x04, 0x08};
    CanFrame f = makeFrame(0x54C, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT16(305, dteValue);
}

void test_dte_zero() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x54C, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT16(0, dteValue);
}

void test_dte_100km() {
    // 338*100/338 = 100
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x52};
    CanFrame f = makeFrame(0x54C, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT16(100, dteValue);
}

void test_dte_deferred_source_does_not_overwrite() {
    uint8_t current[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x52};
    CanFrame currentFrame = makeFrame(0x54C, current, 8);
    TEST_ASSERT_TRUE(proc.processFrame(currentFrame));
    TEST_ASSERT_EQUAL_INT16(100, dteValue);

    // Deferred proposal would decode this 0x2DE payload as 305 km.
    uint8_t proposed[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x0B, 0xEA};
    CanFrame proposedFrame = makeFrame(0x2DE, proposed, 8);
    TEST_ASSERT_FALSE(proc.processFrame(proposedFrame));
    TEST_ASSERT_EQUAL_INT16(100, dteValue);
}

// =============================================================================
// ENGINE RPM — active 0x180 bytes 0-1 BE, raw/7
// =============================================================================

void test_rpm_idle() {
    uint8_t d[] = {0x1C, 0x39, 0x33, 0x43, 0x09, 0x00, 0x31, 0x05};
    CanFrame f = makeFrame(0x180, d, 8);
    proc.processFrame(f);
    // 0x1C39 = 7225, integer 7225/7 = 1032 RPM; no physical validation implied.
    TEST_ASSERT_EQUAL_UINT16(1032, engineRPM);
}

void test_rpm_6000() {
    uint8_t d[] = {0xA4, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x180, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT16(6000, engineRPM);  // 0xA410 = 42000
}

void test_rpm_zero() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x180, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT16(0, engineRPM);
}

// =============================================================================
// VEHICLE SPEED — active 0x284 bytes 0-1 BE, raw/100
// =============================================================================

void test_speed_zero() {
    // Active bytes 0-1 are zero while deferred bytes 4-5 indicate 100 km/h.
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x27, 0x10, 0x00, 0x00};
    CanFrame f = makeFrame(0x284, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT8(0, vehicleSpeed);
}

void test_speed_100kmh() {
    // 100 km/h → raw = 100*100 = 10000 = 0x2710
    // Deferred bytes 4-5 deliberately indicate 50 km/h.
    uint8_t d[] = {0x27, 0x10, 0x00, 0x00, 0x13, 0x88, 0x00, 0x00};
    CanFrame f = makeFrame(0x284, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT8(100, vehicleSpeed);
}

// =============================================================================
// FUEL LEVEL — active 0x5C5 byte0, map(raw, 255, 0, 0, 45)
// Integer results exercise the existing formula, not its physical calibration.
// =============================================================================

void test_fuel_level_empty() {
    uint8_t d[] = {0xFF, 0x03, 0xBA, 0x2C, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x5C5, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT8(0, fuelLevel);
}

void test_fuel_level_max_representable() {
    uint8_t d[] = {0x00, 0x03, 0xBA, 0x2C, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x5C5, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT8(45, fuelLevel);
}

void test_fuel_level_captured_33_liters() {
    uint8_t d[] = {0x40, 0x03, 0xBA, 0x2C, 0x00, 0x0C, 0x00, 0x7F};
    CanFrame f = makeFrame(0x5C5, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT8(33, fuelLevel);  // (255-64)*45/255 = 33
    TEST_ASSERT_EQUAL_UINT32(244268, currentOdo);
}

void test_fuel_level_midpoint() {
    uint8_t d[] = {0x7D, 0x03, 0xBA, 0x2C, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x5C5, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT8(22, fuelLevel);  // (255-125)*45/255 = 22, truncated
}

void test_fuel_level_integer_threshold() {
    uint8_t below[] = {0xFA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame belowFrame = makeFrame(0x5C5, below, 8);
    proc.processFrame(belowFrame);
    TEST_ASSERT_EQUAL_UINT8(0, fuelLevel);  // (255-250)*45/255 = 0

    uint8_t above[] = {0xF9, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame aboveFrame = makeFrame(0x5C5, above, 8);
    proc.processFrame(aboveFrame);
    TEST_ASSERT_EQUAL_UINT8(1, fuelLevel);  // (255-249)*45/255 = 1
}

// =============================================================================
// ODOMETER — 0x5C5 bytes 1-3 UINT24 BE, raw direct
// Validated: 244268 km = 0x03BA2C
// =============================================================================

void test_odometer_244268km() {
    uint8_t d[] = {0x40, 0x03, 0xBA, 0x2C, 0x00, 0x0C, 0x00, 0x7F};
    CanFrame f = makeFrame(0x5C5, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT32(244268, currentOdo);
}

void test_odometer_zero() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x5C5, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT32(0, currentOdo);
}

// =============================================================================
// BATTERY VOLTAGE — 0x6F6 byte0, raw * 0.1f
// Log values: 140-142 → 14.0-14.2V
// =============================================================================

void test_voltage_141() {
    uint8_t d[] = {0x8D, 0x00, 0x00};
    CanFrame f = makeFrame(0x6F6, d, 3);
    proc.processFrame(f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 14.1f, voltBat);  // 0x8D=141, 141*0.1=14.1V
}

// =============================================================================
// STEERING — 0x002 bytes 1-2 BE signed INT16, formula NONE
// Log: -255 / -256 (slight left turn when parked)
// =============================================================================

void test_steering_centered() {
    uint8_t d[] = {0xF7, 0x00, 0x00, 0x07, 0xC3};
    CanFrame f = makeFrame(0x002, d, 5);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT16(0, currentSteer);  // 0x0000 = 0
}

void test_steering_left() {
    uint8_t d[] = {0xF7, 0xFF, 0x01, 0x07, 0xC3};
    CanFrame f = makeFrame(0x002, d, 5);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT16(-255, currentSteer);  // 0xFF01 signed = -255
}

// =============================================================================
// FUEL CONSUMPTION — active 0x580 byte3 (inst), byte4 (avg), direct
// =============================================================================

void test_fuel_cons_inst() {
    uint8_t d[] = {0x00, 0xBE, 0x80, 0x04, 0x62, 0x00, 0x00, 0x54};
    CanFrame f = makeFrame(0x580, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT16(4, fuelConsumptionInst);   // byte3=4, not byte1=0xBE
    TEST_ASSERT_EQUAL_UINT16(98, fuelConsumptionAvg);   // byte4=0x62=98
}

void test_fuel_cons_inst_zero() {
    uint8_t d[] = {0x00, 0xBE, 0x80, 0x00, 0x62, 0x00, 0x00, 0x05};
    CanFrame f = makeFrame(0x580, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT16(0, fuelConsumptionInst);
}

void test_fuel_cons_inst_preserves_full_byte() {
    uint8_t d[] = {0x00, 0x04, 0x80, 0xFF, 0x62, 0x00, 0x00, 0x05};
    CanFrame f = makeFrame(0x580, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT16(255, fuelConsumptionInst);
    TEST_ASSERT_EQUAL_UINT16(98, fuelConsumptionAvg);
}

// =============================================================================
// DOORS & LIGHTS — 0x60D bytes 0-2 BE bitmask
// =============================================================================

void test_doors_all_closed() {
    uint8_t d[] = {0x06, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x60D, d, 8);
    proc.processFrame(f);
    // Bits 23-19 for doors → all 0 in 0x060600
    TEST_ASSERT_EQUAL_UINT8(0, currentDoors & 0xF8);
}

void test_door_driver_open() {
    // Bit 20 = 0x100000 → in byte0 bit 4 → 0x060600 | 0x100000 = 0x160600
    uint8_t d[] = {0x16, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x60D, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_NOT_EQUAL(0, currentDoors & 0x80);  // MASK_DOOR_DRIVER
}

void test_headlights_on() {
    // HEADLIGHTS = bit 17 = 0x020000 → byte0 bit 1 set → byte0=0x02
    uint8_t d[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x60D, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_TRUE(headlightsOn);
}

void test_high_beam_on() {
    // HIGH_BEAM = bit 11 = 0x000800 → byte1 bit 3 set → byte1=0x08
    uint8_t d[] = {0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x60D, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_TRUE(highBeamOn);
}

void test_indicator_left_fires() {
    // INDICATOR_LEFT = bit 13 = 0x002000 → byte1 bit 5 set → byte1=0x20
    uint8_t d[] = {0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x60D, d, 8);
    proc.processFrame(f);
    // lastLeftIndicatorTime gets set to millis() (=0 in mock) when bit is active
    TEST_ASSERT_EQUAL_UINT32(0, lastLeftIndicatorTime);
}

// =============================================================================
// FRAME LENGTH — incomplete fields must not consume padding or update outputs
// =============================================================================

void test_speed_short_frame_preserves_previous_value() {
    uint8_t valid[] = {0x1F, 0x40};
    CanFrame validFrame = makeFrame(0x284, valid, 2);
    TEST_ASSERT_TRUE(proc.processFrame(validFrame));
    TEST_ASSERT_EQUAL_UINT8(80, vehicleSpeed);
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();

    uint8_t truncated[] = {0x27};
    CanFrame truncatedFrame = makeFrame(0x284, truncated, 1);
    TEST_ASSERT_FALSE(proc.processFrame(truncatedFrame));
    TEST_ASSERT_EQUAL_UINT8(80, vehicleSpeed);
    TEST_ASSERT_EQUAL_UINT32(processedBefore, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore, proc.getUnknownFrames());
}

void test_speed_two_byte_frame_accepted() {
    uint8_t d[] = {0x27, 0x10};
    CanFrame f = makeFrame(0x284, d, 2);
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();
    TEST_ASSERT_TRUE(proc.processFrame(f));
    TEST_ASSERT_EQUAL_UINT8(100, vehicleSpeed);
    TEST_ASSERT_EQUAL_UINT32(processedBefore + 1, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore, proc.getUnknownFrames());
}

void test_dte_short_frame_preserves_previous_value() {
    uint8_t valid[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x04, 0x08};
    CanFrame validFrame = makeFrame(0x54C, valid, 8);
    TEST_ASSERT_TRUE(proc.processFrame(validFrame));
    TEST_ASSERT_EQUAL_INT16(305, dteValue);
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();

    CanFrame truncatedFrame = makeFrame(0x54C, valid, 6);
    TEST_ASSERT_FALSE(proc.processFrame(truncatedFrame));
    TEST_ASSERT_EQUAL_INT16(305, dteValue);
    TEST_ASSERT_EQUAL_UINT32(processedBefore, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore, proc.getUnknownFrames());
}

void test_fuel_short_frame_rejected_atomically() {
    uint8_t valid[] = {0x7D, 0x03, 0xBA, 0x2C};
    CanFrame validFrame = makeFrame(0x5C5, valid, 4);
    TEST_ASSERT_TRUE(proc.processFrame(validFrame));
    TEST_ASSERT_EQUAL_UINT8(22, fuelLevel);
    TEST_ASSERT_EQUAL_UINT32(244268, currentOdo);
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();

    // Fuel byte is present, odometer is missing: neither field may be written.
    uint8_t truncated[] = {0x00};
    CanFrame truncatedFrame = makeFrame(0x5C5, truncated, 1);
    TEST_ASSERT_FALSE(proc.processFrame(truncatedFrame));
    TEST_ASSERT_EQUAL_UINT8(22, fuelLevel);
    TEST_ASSERT_EQUAL_UINT32(244268, currentOdo);
    TEST_ASSERT_EQUAL_UINT32(processedBefore, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore, proc.getUnknownFrames());
}

void test_oversized_dlc_rejected() {
    engineRPM = 903;
    // makeFrame copies at most eight bytes even though the DLC is invalid.
    uint8_t d[] = {0xBB, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x180, d, 9);
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();
    TEST_ASSERT_FALSE(proc.processFrame(f));
    TEST_ASSERT_EQUAL_UINT16(903, engineRPM);
    TEST_ASSERT_EQUAL_UINT32(processedBefore, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore, proc.getUnknownFrames());
}

void test_invalid_field_lengths_rejected() {
    LittleFS.basePath = "test/test_can_decoding";
    TEST_ASSERT_TRUE(proc.loadFromJson("/invalid_field_lengths.json"));
    engineRPM = 903;
    vehicleSpeed = 80;
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();
    uint8_t d[] = {0x27, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    CanFrame emptyFieldFrame = makeFrame(0x180, d, 8);
    TEST_ASSERT_FALSE(proc.processFrame(emptyFieldFrame));
    TEST_ASSERT_EQUAL_UINT16(903, engineRPM);

    // startByte=255 plus byteCount=2 must not wrap around to a valid offset.
    CanFrame overflowingFieldFrame = makeFrame(0x284, d, 8);
    TEST_ASSERT_FALSE(proc.processFrame(overflowingFieldFrame));
    TEST_ASSERT_EQUAL_UINT8(80, vehicleSpeed);
    TEST_ASSERT_EQUAL_UINT32(processedBefore, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore, proc.getUnknownFrames());
}

// =============================================================================
// DEFERRED PROPOSALS — preserved outside data/, never loaded by the firmware
// These tests check proposal arithmetic only, not vehicle signal calibration.
// =============================================================================

static void loadDeferredProposals() {
    LittleFS.basePath = "test/fixtures";
    TEST_ASSERT_TRUE(proc.loadFromJson("/proposed_anyzlo_mappings.json"));
}

void test_deferred_rpm_divisor() {
    loadDeferredProposals();
    uint8_t d[] = {0x1C, 0x39, 0x33, 0x43, 0x09, 0x00, 0x31, 0x05};
    CanFrame f = makeFrame(0x180, d, 8);
    TEST_ASSERT_TRUE(proc.processFrame(f));
    TEST_ASSERT_EQUAL_UINT16(903, engineRPM);
}

void test_deferred_speed_offset() {
    loadDeferredProposals();
    uint8_t d[] = {0x13, 0x88, 0x00, 0x00, 0x27, 0x10};
    CanFrame f = makeFrame(0x284, d, 6);
    TEST_ASSERT_TRUE(proc.processFrame(f));
    TEST_ASSERT_EQUAL_UINT8(100, vehicleSpeed);
}

void test_deferred_fuel_formula_bounds() {
    loadDeferredProposals();
    const uint8_t raw[] = {255, 247, 246, 125, 64, 0};
    const uint8_t expected[] = {0, 0, 1, 16, 23, 31};
    for (size_t i = 0; i < sizeof(raw); ++i) {
        uint8_t d[] = {raw[i], 0x03, 0xBA, 0x2C};
        CanFrame f = makeFrame(0x5C5, d, 4);
        TEST_ASSERT_TRUE(proc.processFrame(f));
        TEST_ASSERT_EQUAL_UINT8(expected[i], fuelLevel);
        TEST_ASSERT_EQUAL_UINT32(244268, currentOdo);
    }
}

void test_deferred_dte_source_exclusive() {
    loadDeferredProposals();
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0B, 0xEA};
    CanFrame proposedFrame = makeFrame(0x2DE, d, 8);
    TEST_ASSERT_TRUE(proc.processFrame(proposedFrame));
    TEST_ASSERT_EQUAL_INT16(305, dteValue);
    CanFrame activeFrame = makeFrame(0x54C, d, 8);
    TEST_ASSERT_FALSE(proc.processFrame(activeFrame));
    TEST_ASSERT_EQUAL_INT16(305, dteValue);
}

void test_deferred_consumption_offset_full_byte() {
    loadDeferredProposals();
    uint8_t d[] = {0x00, 0xFF, 0x80, 0x04, 0x62};
    CanFrame f = makeFrame(0x580, d, 5);
    TEST_ASSERT_TRUE(proc.processFrame(f));
    TEST_ASSERT_EQUAL_UINT16(255, fuelConsumptionInst);
    TEST_ASSERT_EQUAL_UINT16(98, fuelConsumptionAvg);
}

void test_deferred_speed_short_frame_rejected() {
    loadDeferredProposals();
    vehicleSpeed = 88;
    uint8_t d[] = {0x27, 0x10, 0x00, 0x00};
    CanFrame f = makeFrame(0x284, d, 4);
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();
    TEST_ASSERT_FALSE(proc.processFrame(f));
    TEST_ASSERT_EQUAL_UINT8(88, vehicleSpeed);
    TEST_ASSERT_EQUAL_UINT32(processedBefore, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore, proc.getUnknownFrames());
}

// =============================================================================
// FRAME TYPE — only standard 11-bit data frames can affect vehicle state
// =============================================================================

static void assertUnsupportedFramePreservesState(const CanFrame& frame) {
    currentSteer = -255; engineRPM = 1032; vehicleSpeed = 88;
    currentDoors = 0x80; fuelLevel = 33; voltBat = 14.1f;
    dteValue = 305; tempExt = 28; coolantTemp = 50; currentOdo = 239944;
    headlightsOn = true; highBeamOn = true; parkingLightsOn = true;
    fuelConsumptionInst = 65; fuelConsumptionAvg = 98;
    lastLeftIndicatorTime = 111; lastRightIndicatorTime = 222;
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();

    TEST_ASSERT_FALSE(proc.processFrame(frame));
    TEST_ASSERT_EQUAL_INT16(-255, currentSteer);
    TEST_ASSERT_EQUAL_UINT16(1032, engineRPM);
    TEST_ASSERT_EQUAL_UINT8(88, vehicleSpeed);
    TEST_ASSERT_EQUAL_UINT8(0x80, currentDoors);
    TEST_ASSERT_EQUAL_UINT8(33, fuelLevel);
    TEST_ASSERT_EQUAL_FLOAT(14.1f, voltBat);
    TEST_ASSERT_EQUAL_INT16(305, dteValue);
    TEST_ASSERT_EQUAL_INT8(28, tempExt);
    TEST_ASSERT_EQUAL_INT8(50, coolantTemp);
    TEST_ASSERT_EQUAL_UINT32(239944, currentOdo);
    TEST_ASSERT_TRUE(headlightsOn);
    TEST_ASSERT_TRUE(highBeamOn);
    TEST_ASSERT_TRUE(parkingLightsOn);
    TEST_ASSERT_EQUAL_UINT16(65, fuelConsumptionInst);
    TEST_ASSERT_EQUAL_UINT16(98, fuelConsumptionAvg);
    TEST_ASSERT_EQUAL_UINT32(111, lastLeftIndicatorTime);
    TEST_ASSERT_EQUAL_UINT32(222, lastRightIndicatorTime);
    TEST_ASSERT_EQUAL_UINT32(processedBefore, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore, proc.getUnknownFrames());
}

void test_rtr_configured_id_preserves_state() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x5C5, d, 4);
    f.rtr = true;
    assertUnsupportedFramePreservesState(f);
}

void test_extended_configured_id_preserves_state() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x5C5, d, 4);
    f.extd = true;
    assertUnsupportedFramePreservesState(f);
}

void test_identifier_outside_11_bits_preserves_state() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00};
    // 0x105C5 would alias configured 0x5C5 if narrowed to a 16-bit lookup ID.
    for (const uint32_t id : {0x800u, 0x105C5u}) {
        CanFrame f = makeFrame(id, d, 4);
        assertUnsupportedFramePreservesState(f);
    }
}

// =============================================================================
// UNKNOWN FRAME — processFrame returns false for unconfigured standard IDs
// =============================================================================

void test_unknown_frame_ignored() {
    uint8_t d[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    CanFrame f = makeFrame(0x700, d, 8);
    const uint32_t processedBefore = proc.getFramesProcessed();
    const uint32_t unknownBefore = proc.getUnknownFrames();
    bool result = proc.processFrame(f);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT8(0, tempExt);  // GlobalData unchanged
    TEST_ASSERT_EQUAL_UINT32(processedBefore, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(unknownBefore + 1, proc.getUnknownFrames());
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // External temperature
    RUN_TEST(test_temp_ext_28c);
    RUN_TEST(test_temp_ext_zero);
    RUN_TEST(test_temp_ext_negative);

    // Coolant temperature
    RUN_TEST(test_coolant_50c);
    RUN_TEST(test_coolant_operating_temp);

    // Distance to empty
    RUN_TEST(test_dte_305km);
    RUN_TEST(test_dte_zero);
    RUN_TEST(test_dte_100km);
    RUN_TEST(test_dte_deferred_source_does_not_overwrite);

    // Engine RPM
    RUN_TEST(test_rpm_idle);
    RUN_TEST(test_rpm_6000);
    RUN_TEST(test_rpm_zero);

    // Vehicle speed
    RUN_TEST(test_speed_zero);
    RUN_TEST(test_speed_100kmh);

    // Fuel level
    RUN_TEST(test_fuel_level_empty);
    RUN_TEST(test_fuel_level_max_representable);
    RUN_TEST(test_fuel_level_captured_33_liters);
    RUN_TEST(test_fuel_level_midpoint);
    RUN_TEST(test_fuel_level_integer_threshold);

    // Odometer
    RUN_TEST(test_odometer_244268km);
    RUN_TEST(test_odometer_zero);

    // Voltage
    RUN_TEST(test_voltage_141);

    // Steering
    RUN_TEST(test_steering_centered);
    RUN_TEST(test_steering_left);

    // Fuel consumption
    RUN_TEST(test_fuel_cons_inst);
    RUN_TEST(test_fuel_cons_inst_zero);
    RUN_TEST(test_fuel_cons_inst_preserves_full_byte);

    // Doors & lights
    RUN_TEST(test_doors_all_closed);
    RUN_TEST(test_door_driver_open);
    RUN_TEST(test_headlights_on);
    RUN_TEST(test_high_beam_on);
    RUN_TEST(test_indicator_left_fires);

    // Frame length validation
    RUN_TEST(test_speed_short_frame_preserves_previous_value);
    RUN_TEST(test_speed_two_byte_frame_accepted);
    RUN_TEST(test_dte_short_frame_preserves_previous_value);
    RUN_TEST(test_fuel_short_frame_rejected_atomically);
    RUN_TEST(test_oversized_dlc_rejected);
    RUN_TEST(test_invalid_field_lengths_rejected);

    // Deferred PR review proposals, kept outside the active production preset
    RUN_TEST(test_deferred_rpm_divisor);
    RUN_TEST(test_deferred_speed_offset);
    RUN_TEST(test_deferred_fuel_formula_bounds);
    RUN_TEST(test_deferred_dte_source_exclusive);
    RUN_TEST(test_deferred_consumption_offset_full_byte);
    RUN_TEST(test_deferred_speed_short_frame_rejected);

    // Standard data frames only
    RUN_TEST(test_rtr_configured_id_preserves_state);
    RUN_TEST(test_extended_configured_id_preserves_state);
    RUN_TEST(test_identifier_outside_11_bits_preserves_state);

    // Unknown frame
    RUN_TEST(test_unknown_frame_ignored);

    return UNITY_END();
}
