/**
 * @file test_can_decoding.cpp
 * @brief Regression tests for CAN frame → GlobalData decoding pipeline
 *
 * Loads production config (data/NissanJukeF15.json) via LittleFS mock,
 * feeds raw CAN frames into processFrame(), and asserts GlobalData values.
 * Values validated against real Nissan Juke F15 dashboard readings.
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
    proc.loadFromJson("/NissanJukeF15.json");
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
// DISTANCE TO EMPTY — 0x54C bytes 6-7 BE, raw*100/338
// Validated: dashboard showed 305km, raw=0x0408=1032, 1032*100/338=305
// =============================================================================

void test_dte_305km() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0x04, 0x08};
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
    // 100km → need raw = 100*338/100 = 338 = 0x0152
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x52};
    CanFrame f = makeFrame(0x54C, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_INT16(100, dteValue);  // 338*100/338=100
}

// =============================================================================
// ENGINE RPM — 0x180 bytes 0-1 BE, raw/7
// Expected at idle: raw~7200 → 7200/7=1028 RPM
// =============================================================================

void test_rpm_idle() {
    uint8_t d[] = {0x1C, 0x39, 0x33, 0x43, 0x09, 0x00, 0x31, 0x05};
    CanFrame f = makeFrame(0x180, d, 8);
    proc.processFrame(f);
    // 0x1C39 = 7225, 7225/7 = 1032
    TEST_ASSERT_EQUAL_UINT16(1032, engineRPM);
}

void test_rpm_zero() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x180, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT16(0, engineRPM);
}

// =============================================================================
// VEHICLE SPEED — 0x284 bytes 0-1 BE, raw/100
// =============================================================================

void test_speed_zero() {
    uint8_t d[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x284, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT8(0, vehicleSpeed);
}

void test_speed_100kmh() {
    // 100 km/h → raw = 100*100 = 10000 = 0x2710
    uint8_t d[] = {0x27, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    CanFrame f = makeFrame(0x284, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT8(100, vehicleSpeed);
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
// FUEL CONSUMPTION — 0x580 byte3 (inst), byte4 (avg), formula NONE
// =============================================================================

void test_fuel_cons_inst() {
    uint8_t d[] = {0x00, 0xBE, 0x80, 0x04, 0x62, 0x00, 0x00, 0x54};
    CanFrame f = makeFrame(0x580, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT16(4, fuelConsumptionInst);   // byte3=0x04
    TEST_ASSERT_EQUAL_UINT16(98, fuelConsumptionAvg);   // byte4=0x62=98
}

void test_fuel_cons_inst_zero() {
    uint8_t d[] = {0x00, 0xBE, 0x80, 0x00, 0x62, 0x00, 0x00, 0x05};
    CanFrame f = makeFrame(0x580, d, 8);
    proc.processFrame(f);
    TEST_ASSERT_EQUAL_UINT16(0, fuelConsumptionInst);
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
// UNKNOWN FRAME — processFrame returns false for unconfigured IDs
// =============================================================================

void test_unknown_frame_ignored() {
    uint8_t d[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    CanFrame f = makeFrame(0x999, d, 8);
    bool result = proc.processFrame(f);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT8(0, tempExt);  // GlobalData unchanged
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

    // Engine RPM
    RUN_TEST(test_rpm_idle);
    RUN_TEST(test_rpm_zero);

    // Vehicle speed
    RUN_TEST(test_speed_zero);
    RUN_TEST(test_speed_100kmh);

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

    // Doors & lights
    RUN_TEST(test_doors_all_closed);
    RUN_TEST(test_door_driver_open);
    RUN_TEST(test_headlights_on);
    RUN_TEST(test_high_beam_on);
    RUN_TEST(test_indicator_left_fires);

    // Unknown frame
    RUN_TEST(test_unknown_frame_ignored);

    return UNITY_END();
}
