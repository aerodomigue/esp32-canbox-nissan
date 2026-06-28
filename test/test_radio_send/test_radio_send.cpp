/**
 * @file test_radio_send.cpp
 * @brief Tests for RadioSend UART encoding — Toyota RAV4 Raise protocol
 *
 * Each test calls a send function and verifies the exact bytes written to
 * RadioSerial (captured in HardwareSerial::captured vector).
 * Expected frames pre-computed: header=0x2E, cmd, len, payload, checksum
 * where checksum = (cmd + len + Σpayload) ^ 0xFF
 *
 * Protocol reference: RAV4_BODY_ENGINE_2018_2020.pdf (Mikescotland)
 *                     睿志诚软件_丰田CAMRY & RAV4 串口通讯协议 V2.21.000
 *
 * Run: pio test -e native_radio
 */

#include <unity.h>
#include <vector>
#include "Arduino.h"
#include "RadioSend.h"

// Forward declarations — functions defined in RadioSend.cpp but not exported in header
void sendDoorCommand(uint8_t doorMask, bool igOn);
void sendSteeringAngleMessage(int16_t angle);
void sendRpmMessage(uint16_t rpm);
void sendSpeedMessage(uint16_t speed);
void sendOdometerMessage(uint32_t odo);
void sendOutsideTempMessage(int8_t tempExternal, int8_t tempCoolant);
void sendTripInfoMessage(uint16_t range_km, uint16_t avg_speed_01, uint16_t elapsed_sec);
void sendFuelConsumptionMessage(uint16_t consumption_01);
void sendFuelConsumptionAvgMessage(uint16_t consumption_01);
void sendLightsMessage(uint8_t lightMask);

extern HardwareSerial RadioSerial;

// ── helpers ──────────────────────────────────────────────────────────────────

static void assertFrame(const uint8_t* expected, size_t len) {
    TEST_ASSERT_EQUAL_MESSAGE(len, RadioSerial.captured.size(), "frame length mismatch");
    for (size_t i = 0; i < len; i++) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(expected[i], RadioSerial.captured[i], "byte mismatch");
    }
}

void setUp()    { RadioSerial.clear(); }
void tearDown() {}

// =============================================================================
// CMD 0x24 — Door status + IG key
// Per Raise Chinese spec V2.21: 2-byte payload [doors, key]
// Bit1 of key byte = IG ON (engineRPM > 0)
// =============================================================================

void test_door_driver_open_ig_on() {
    sendDoorCommand(0x80, true);
    // 2E 24 02 80 02 57
    uint8_t exp[] = {0x2E, 0x24, 0x02, 0x80, 0x02, 0x57};
    assertFrame(exp, sizeof(exp));
}

void test_door_all_closed_ig_off() {
    sendDoorCommand(0x00, false);
    // 2E 24 02 00 00 D9
    uint8_t exp[] = {0x2E, 0x24, 0x02, 0x00, 0x00, 0xD9};
    assertFrame(exp, sizeof(exp));
}

void test_door_multiple_open_ig_on() {
    // driver(0x80) + passenger(0x40) + boot(0x08) = 0xC8, ig=true → key=0x02
    sendDoorCommand(0xC8, true);
    // sum = 0x24+0x02+0xC8+0x02 = 0xF0, cs = 0x0F
    uint8_t exp[] = {0x2E, 0x24, 0x02, 0xC8, 0x02, 0x0F};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x29 — Steering wheel angle (0.1° units, signed LE)
// =============================================================================

void test_steering_zero() {
    sendSteeringAngleMessage(0);
    // 2E 29 02 00 00 D4
    uint8_t exp[] = {0x2E, 0x29, 0x02, 0x00, 0x00, 0xD4};
    assertFrame(exp, sizeof(exp));
}

void test_steering_minus250() {
    // -250 → 0xFF06 → LSB=0x06, MSB=0xFF
    sendSteeringAngleMessage(-250);
    // 2E 29 02 06 FF CF
    uint8_t exp[] = {0x2E, 0x29, 0x02, 0x06, 0xFF, 0xCF};
    assertFrame(exp, sizeof(exp));
}

void test_steering_positive() {
    // +250 → 0x00FA → LSB=0xFA, MSB=0x00
    sendSteeringAngleMessage(250);
    // sum=0x29+0x02+0xFA+0x00=0x125, cs=(0x25^0xFF)=0xDA
    uint8_t exp[] = {0x2E, 0x29, 0x02, 0xFA, 0x00, 0xDA};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x7D sub 0x0A — Engine RPM (×4, LE)
// =============================================================================

void test_rpm_zero() {
    sendRpmMessage(0);
    // 2E 7D 03 0A 00 00 75
    uint8_t exp[] = {0x2E, 0x7D, 0x03, 0x0A, 0x00, 0x00, 0x75};
    assertFrame(exp, sizeof(exp));
}

void test_rpm_1000() {
    // 1000×4=4000=0x0FA0 → LSB=0xA0 MSB=0x0F
    sendRpmMessage(1000);
    // 2E 7D 03 0A A0 0F C6
    uint8_t exp[] = {0x2E, 0x7D, 0x03, 0x0A, 0xA0, 0x0F, 0xC6};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x7D sub 0x03 — Vehicle speed (×100, LE) + avg speed
// =============================================================================

void test_speed_zero() {
    sendSpeedMessage(0);
    // 2E 7D 05 03 00 00 00 00 7A
    uint8_t exp[] = {0x2E, 0x7D, 0x05, 0x03, 0x00, 0x00, 0x00, 0x00, 0x7A};
    assertFrame(exp, sizeof(exp));
}

void test_speed_50kmh() {
    // 50×100=5000=0x1388 → LSB=0x88 MSB=0x13
    sendSpeedMessage(50);
    // 2E 7D 05 03 88 13 00 00 DF
    uint8_t exp[] = {0x2E, 0x7D, 0x05, 0x03, 0x88, 0x13, 0x00, 0x00, 0xDF};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x7D sub 0x04 — Odometer (24-bit LE) + magic bytes F2 08
// =============================================================================

void test_odometer_244268km() {
    // 244268 = 0x03BA2C → LSB=0x2C, Mid=0xBA, MSB=0x03
    sendOdometerMessage(244268);
    // 2E 7D 0C 04 2C BA 03 F2 08 00 00 00 00 00 00 8F
    uint8_t exp[] = {0x2E, 0x7D, 0x0C, 0x04,
                     0x2C, 0xBA, 0x03,          // odometer LE
                     0xF2, 0x08,                 // unknown/reserved
                     0x00, 0x00, 0x00,           // trip1
                     0x00, 0x00, 0x00,           // trip2
                     0x8F};
    assertFrame(exp, sizeof(exp));
}

void test_odometer_zero() {
    sendOdometerMessage(0);
    // sum=(0x7D+0x0C+0x04+0xF2+0x08)&0xFF=0x87, cs=0x78
    uint8_t exp[] = {0x2E, 0x7D, 0x0C, 0x04,
                     0x00, 0x00, 0x00,
                     0xF2, 0x08,
                     0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00,
                     0x78};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x28 — Outside temp (byte[5]) + coolant hypothesis (byte[0])
// Encoding: (temp+40)×2
// =============================================================================

void test_temp_28c_coolant_50c() {
    // ext=28: (28+40)×2=136=0x88 at byte[5]
    // coolant=50: (50+40)×2=180=0xB4 at byte[0]
    sendOutsideTempMessage(28, 50);
    // 2E 28 0C B4 00 00 00 00 88 00 00 00 00 00 00 8F
    uint8_t exp[] = {0x2E, 0x28, 0x0C,
                     0xB4,                              // byte[0] coolant
                     0x00, 0x00, 0x00, 0x00,
                     0x88,                              // byte[5] ext temp
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x8F};
    assertFrame(exp, sizeof(exp));
}

void test_temp_minus10c_coolant_0c() {
    // ext=-10: (-10+40)×2=60=0x3C; coolant=0: (0+40)×2=80=0x50
    sendOutsideTempMessage(-10, 0);
    // 2E 28 0C 50 00 00 00 00 3C 00 00 00 00 00 00 3F
    uint8_t exp[] = {0x2E, 0x28, 0x0C,
                     0x50, 0x00, 0x00, 0x00, 0x00,
                     0x3C,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x3F};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x21 — Trip info: avg speed + elapsed time + remaining range (km)
// =============================================================================

void test_trip_305km_range() {
    // range=305=0x0131 → MSB=0x01, LSB=0x31
    sendTripInfoMessage(305, 0, 0);
    // 2E 21 07 00 00 00 00 01 31 02 A3
    uint8_t exp[] = {0x2E, 0x21, 0x07,
                     0x00, 0x00,   // avg speed
                     0x00, 0x00,   // elapsed
                     0x01, 0x31,   // range
                     0x02,         // unit km
                     0xA3};
    assertFrame(exp, sizeof(exp));
}

void test_trip_zero() {
    sendTripInfoMessage(0, 0, 0);
    // sum=0x21+0x07+0x02=0x2A, cs=0xD5
    uint8_t exp[] = {0x2E, 0x21, 0x07,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x02,
                     0xD5};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x22 — Instantaneous fuel consumption (0.1 L/100km, L/100km unit=0x02)
// PDF example: 6.5 L/100km → value=65=0x0041
// =============================================================================

void test_fuel_inst_6_5_l100km() {
    sendFuelConsumptionMessage(65);  // 6.5 L/100km
    // 2E 22 03 02 00 41 97  (matches PDF example)
    uint8_t exp[] = {0x2E, 0x22, 0x03, 0x02, 0x00, 0x41, 0x97};
    assertFrame(exp, sizeof(exp));
}

void test_fuel_inst_zero() {
    sendFuelConsumptionMessage(0);
    // sum=0x22+0x03+0x02=0x27, cs=0xD8
    uint8_t exp[] = {0x2E, 0x22, 0x03, 0x02, 0x00, 0x00, 0xD8};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x23 — Average fuel consumption (same encoding as 0x22)
// =============================================================================

void test_fuel_avg_9_8_l100km() {
    sendFuelConsumptionAvgMessage(98);  // 9.8 L/100km
    // 2E 23 03 02 00 62 75
    uint8_t exp[] = {0x2E, 0x23, 0x03, 0x02, 0x00, 0x62, 0x75};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CMD 0x7D sub 0x01 — Lights bitmask
// 0x08=right ind, 0x10=left ind, 0x20=high beam, 0x40=headlights, 0x80=parking
// =============================================================================

void test_lights_off() {
    sendLightsMessage(0x00);
    // 2E 7D 02 01 00 7F
    uint8_t exp[] = {0x2E, 0x7D, 0x02, 0x01, 0x00, 0x7F};
    assertFrame(exp, sizeof(exp));
}

void test_lights_headlights_only() {
    sendLightsMessage(0x40);
    // 2E 7D 02 01 40 3F
    uint8_t exp[] = {0x2E, 0x7D, 0x02, 0x01, 0x40, 0x3F};
    assertFrame(exp, sizeof(exp));
}

void test_lights_all_on() {
    // headlights+parking+left+right+highbeam = 0xF8
    sendLightsMessage(0xF8);
    // sum=0x7D+0x02+0x01+0xF8=0x178, cs=(0x78^0xFF)=0x87
    uint8_t exp[] = {0x2E, 0x7D, 0x02, 0x01, 0xF8, 0x87};
    assertFrame(exp, sizeof(exp));
}

void test_lights_pdf_example() {
    // PDF example: headlights+parking+left ind = 0x40|0x80|0x10 = 0xD0
    // Final frame: 2E 7D 02 01 D0 AF
    sendLightsMessage(0xD0);
    uint8_t exp[] = {0x2E, 0x7D, 0x02, 0x01, 0xD0, 0xAF};
    assertFrame(exp, sizeof(exp));
}

// =============================================================================
// CHECKSUM — verify (cmd + len + Σdata) ^ 0xFF property
// =============================================================================

void test_checksum_property() {
    // Any frame: header + cmd + len + payload + cs
    // Verify: (sum of bytes 1..N-1) ^ 0xFF == last byte
    sendDoorCommand(0x48, true);  // rear_left(0x10)+rear_right(0x20)+boot(0x08)=0x38... actually 0x48=boot+passenger
    auto& cap = RadioSerial.captured;
    TEST_ASSERT_EQUAL(6, (int)cap.size());
    uint8_t sum = 0;
    for (size_t i = 1; i < cap.size() - 1; i++) sum += cap[i];
    TEST_ASSERT_EQUAL_HEX8(sum ^ 0xFF, cap.back());
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Door status + IG
    RUN_TEST(test_door_driver_open_ig_on);
    RUN_TEST(test_door_all_closed_ig_off);
    RUN_TEST(test_door_multiple_open_ig_on);

    // Steering
    RUN_TEST(test_steering_zero);
    RUN_TEST(test_steering_minus250);
    RUN_TEST(test_steering_positive);

    // RPM
    RUN_TEST(test_rpm_zero);
    RUN_TEST(test_rpm_1000);

    // Speed
    RUN_TEST(test_speed_zero);
    RUN_TEST(test_speed_50kmh);

    // Odometer
    RUN_TEST(test_odometer_244268km);
    RUN_TEST(test_odometer_zero);

    // Temperature
    RUN_TEST(test_temp_28c_coolant_50c);
    RUN_TEST(test_temp_minus10c_coolant_0c);

    // Trip info
    RUN_TEST(test_trip_305km_range);
    RUN_TEST(test_trip_zero);

    // Fuel consumption
    RUN_TEST(test_fuel_inst_6_5_l100km);
    RUN_TEST(test_fuel_inst_zero);
    RUN_TEST(test_fuel_avg_9_8_l100km);

    // Lights
    RUN_TEST(test_lights_off);
    RUN_TEST(test_lights_headlights_only);
    RUN_TEST(test_lights_all_on);
    RUN_TEST(test_lights_pdf_example);

    // Checksum property
    RUN_TEST(test_checksum_property);

    return UNITY_END();
}
