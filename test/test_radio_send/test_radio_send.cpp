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
#include "GlobalData.h"

// Forward declarations — functions defined in RadioSend.cpp but not exported in header
void sendDoorCommand(uint8_t doorMask, bool igOn);
void sendSteeringAngleMessage(int32_t angle);
void sendRpmMessage(uint16_t rpm);
void sendSpeedMessage(uint16_t speedCentiKmh, uint16_t averageCentiKmh);
void sendOdometerMessage(uint32_t odo, uint32_t tripDeciKm);
void sendOutsideTempMessage(int8_t tempExternal);
void sendTemperatureMessage(int16_t coolant, int16_t oil);
void sendTripInfoMessage(uint16_t range_km, uint16_t avg_speed_01, uint16_t elapsed_minutes);
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

void setUp()    { RadioSerial.clear(); canAuxData = {}; }
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
// CMD 0x29 — Calibrated UART steering units, signed LE, saturated at +/-500
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

void test_steering_saturates_before_narrowing() {
    const uint8_t positive[] = {0x2E, 0x29, 0x02, 0xF4, 0x01, 0xDF};  // +500
    const uint8_t negative[] = {0x2E, 0x29, 0x02, 0x0C, 0xFE, 0xCA};  // -500
    for (int32_t value : {500, 501, 2048, 65534, INT32_MAX}) {
        RadioSerial.clear();
        sendSteeringAngleMessage(value);
        assertFrame(positive, sizeof(positive));
    }
    for (int32_t value : {-500, -501, -2049, -65536, INT32_MIN}) {
        RadioSerial.clear();
        sendSteeringAngleMessage(value);
        assertFrame(negative, sizeof(negative));
    }
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

void test_rpm_rejects_receiver_range_and_multiplication_wrap() {
    for (uint16_t rpm : {15000, 16384, 65535}) sendRpmMessage(rpm);
    TEST_ASSERT_EQUAL(0, RadioSerial.captured.size());
}

// =============================================================================
// CMD 0x7D sub 0x03 — Vehicle speed (×100, LE) + avg speed
// =============================================================================

void test_speed_zero() {
    sendSpeedMessage(0, 0);
    // Explicit metric unit D5; checksum remains outside all DATA bytes.
    uint8_t exp[] = {0x2E, 0x7D, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x79};
    assertFrame(exp, sizeof(exp));
}

void test_speed_50kmh() {
    // 50×100=5000=0x1388 → LSB=0x88 MSB=0x13
    sendSpeedMessage(5000, 0);
    // 50.00 km/h, no boot distance yet, explicit metric unit.
    uint8_t exp[] = {0x2E, 0x7D, 0x06, 0x03, 0x88, 0x13, 0x00, 0x00, 0x00, 0xDE};
    assertFrame(exp, sizeof(exp));
}

void test_speed_fractional_and_boot_average_with_explicit_unit() {
    sendSpeedMessage(9321, 5618);
    // Independent offline decoder vector: 93.21 km/h and 56.18 km/h.
    const uint8_t exp[] = {0x2E, 0x7D, 0x06, 0x03, 0x69, 0x24, 0xF2, 0x15, 0x00, 0xE5};
    assertFrame(exp, sizeof(exp));
}

void test_speed_rejects_values_outside_receiver_range() {
    sendSpeedMessage(30000, 0);
    sendSpeedMessage(65535, 0);
    sendSpeedMessage(100, 30000);
    TEST_ASSERT_EQUAL(0, RadioSerial.captured.size());
}

// =============================================================================
// CMD 0x7D sub 0x04 — Odometer (24-bit LE) + magic bytes F2 08
// =============================================================================

void test_odometer_244268km() {
    // 244268 = 0x03BA2C → LSB=0x2C, Mid=0xBA, MSB=0x03
    sendOdometerMessage(244268, 0);
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
    sendOdometerMessage(0, 0);
    // sum=(0x7D+0x0C+0x04+0xF2+0x08)&0xFF=0x87, cs=0x78
    uint8_t exp[] = {0x2E, 0x7D, 0x0C, 0x04,
                     0x00, 0x00, 0x00,
                     0xF2, 0x08,
                     0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00,
                     0x78};
    assertFrame(exp, sizeof(exp));
}

void test_odometer_boot_trip_mirrored_with_deci_kilometres() {
    sendOdometerMessage(247498, 149);
    const auto& b = RadioSerial.captured;
    TEST_ASSERT_EQUAL(16, b.size());
    TEST_ASSERT_EQUAL_HEX8(12, b[2]);
    TEST_ASSERT_EQUAL_UINT32(247498, uint32_t(b[4]) | uint32_t(b[5]) << 8 | uint32_t(b[6]) << 16);
    TEST_ASSERT_EQUAL_HEX8(0xF2, b[7]); // unexplained legacy bytes preserved
    TEST_ASSERT_EQUAL_HEX8(0x08, b[8]);
    for (size_t start : {size_t(9), size_t(12)}) {
        const uint32_t raw = uint32_t(b[start]) | uint32_t(b[start+1]) << 8 |
                             uint32_t(b[start+2]) << 16;
        TEST_ASSERT_EQUAL_UINT32(149, raw); // 14.9 km in each virtual trip slot
    }
}

void test_odometer_rejects_narrowing_wrap() {
    sendOdometerMessage(0x1000000, 0);
    sendOdometerMessage(247498, 0x1000000);
    TEST_ASSERT_EQUAL(0, RadioSerial.captured.size());
}

// =============================================================================
// CMD 0x28 — Outside temp only (byte[5]); byte[0] is climate status
// Encoding: (temp+40)×2
// =============================================================================

void test_temp_28c_without_false_climate_status() {
    // ext=28: (28+40)×2=136=0x88 at byte[5]
    sendOutsideTempMessage(28);
    uint8_t exp[] = {0x2E, 0x28, 0x0C,
                     0x00,                              // no invented A/C status
                     0x00, 0x00, 0x00, 0x00,
                     0x88,                              // byte[5] ext temp
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x43};
    assertFrame(exp, sizeof(exp));
}

void test_temp_minus10c() {
    sendOutsideTempMessage(-10);
    uint8_t exp[] = {0x2E, 0x28, 0x0C,
                     0x00, 0x00, 0x00, 0x00, 0x00,
                     0x3C,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x8F};
    assertFrame(exp, sizeof(exp));
}

void test_coolant_wire_85c() {
    sendTemperatureMessage(85, 95);
    const uint8_t expected[] = {0x2E, 0x7D, 0x04, 0x09, 0x7D, 0x7D, 0x87, 0xF4};
    assertFrame(expected, sizeof(expected));
}

void test_coolant_wire_complete_range_without_wrapping() {
    // Independently decode the exact unsigned-byte-minus-40 format read by c3.
    for (int temp = -40; temp <= 215; ++temp) {
        RadioSerial.clear();
        sendTemperatureMessage(temp, 90);
        const auto& b = RadioSerial.captured;
        TEST_ASSERT_EQUAL(8, b.size());
        TEST_ASSERT_EQUAL_HEX8(0x7D, b[1]);
        TEST_ASSERT_EQUAL_HEX8(4, b[2]);
        TEST_ASSERT_EQUAL_HEX8(9, b[3]);
        TEST_ASSERT_EQUAL_INT(temp, int(b[4]) - 40);
        TEST_ASSERT_EQUAL_INT(temp, int(b[5]) - 40);
        TEST_ASSERT_EQUAL_INT(90, int(b[6]) - 40);
        TEST_ASSERT_EQUAL_HEX8(0xFF, uint8_t(b[1]+b[2]+b[3]+b[4]+b[5]+b[6]+b[7]));
    }
    for (int oil = -40; oil <= 215; ++oil) {
        RadioSerial.clear();
        sendTemperatureMessage(85, oil);
        TEST_ASSERT_EQUAL(8, RadioSerial.captured.size());
        TEST_ASSERT_EQUAL_INT(oil, int(RadioSerial.captured[6]) - 40);
    }
}

void test_coolant_wire_rejects_unrepresentable_values() {
    for (int16_t temp : {-32768, -41, 216, 32767}) {
        sendTemperatureMessage(temp, 90);
        sendTemperatureMessage(85, temp);
    }
    TEST_ASSERT_EQUAL(0, RadioSerial.captured.size());
}

static unsigned coolantFrames() {
    const auto& b = RadioSerial.captured;
    unsigned count = 0;
    for (size_t i = 0; i < b.size();) {
        TEST_ASSERT_TRUE(i + 4 <= b.size());
        TEST_ASSERT_EQUAL_HEX8(0x2E, b[i]);
        const size_t next = i + 4 + b[i+2];
        TEST_ASSERT_TRUE(next <= b.size());
        if (b[i+1] == 0x7D && b[i+2] && b[i+3] == 0x09) ++count;
        if (b[i+1] == 0x28) TEST_ASSERT_EQUAL_HEX8(0, b[i+3]);
        i = next;
    }
    return count;
}

void test_coolant_scheduler_requires_received_fresh_source() {
    canAuxData = {};
    canAuxData.duduReceiver = true;
    coolantTemp = 85;
    mockMillis = 5000;
    processRadioUpdates();
    TEST_ASSERT_EQUAL(0, coolantFrames()); // not received, despite stored value

    RadioSerial.clear();
    mockMillis = 10000;
    canAuxData.coolantValid = true;
    canAuxData.coolantReceivedAt = mockMillis;
    processRadioUpdates();
    TEST_ASSERT_EQUAL(0, coolantFrames()); // no oil: do not invent a companion

    RadioSerial.clear();
    mockMillis = 15000;
    canAuxData.coolantValid = true;
    canAuxData.coolantReceivedAt = 13001;
    canAuxData.oilValid = true;
    canAuxData.oilReceivedAt = 13001;
    canAuxData.oilTemperatureC = 90;
    coolantTemp = 0; // 0°C is a measurement, not an unknown sentinel
    processRadioUpdates();
    TEST_ASSERT_EQUAL(1, coolantFrames());

    RadioSerial.clear();
    mockMillis = 20000;
    canAuxData.coolantReceivedAt = mockMillis;
    canAuxData.oilReceivedAt = 18000;
    processRadioUpdates();
    TEST_ASSERT_EQUAL(0, coolantFrames()); // oil exactly 2 s old, coolant fresh

    RadioSerial.clear();
    mockMillis = 25000;
    canAuxData.coolantReceivedAt = mockMillis;
    canAuxData.coolantValid = false;
    processRadioUpdates();
    TEST_ASSERT_EQUAL(0, coolantFrames()); // profile reload / invalid sample

    // A fresh source across the 32-bit millis wrap must still be transmitted.
    RadioSerial.clear();
    mockMillis = 1000;
    canAuxData.coolantValid = true;
    canAuxData.coolantReceivedAt = UINT32_MAX - 500;
    canAuxData.oilReceivedAt = UINT32_MAX - 500;
    processRadioUpdates();
    TEST_ASSERT_EQUAL(1, coolantFrames());
    canAuxData = {};
    mockMillis = 0;
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

void test_trip_elapsed_is_minutes_not_seconds() {
    sendTripInfoMessage(400, 255, 90);
    const uint8_t exp[] = {0x2E, 0x21, 0x07, 0x00, 0xFF, 0x00, 0x5A, 0x01, 0x90, 0x02, 0xEB};
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

void test_fuel_instant_ffff_is_not_a_dudu_unavailable_value() {
    sendFuelConsumptionMessage(0xFFFF);
    TEST_ASSERT_EQUAL(0, RadioSerial.captured.size());
}

// =============================================================================
// CMD 0x23 — Current trip + five historical trips (Raise V2.21 page 5)
// Payload length is 13; unavailable trips use 0xFFFF, not a zero consumption.
// =============================================================================

void test_fuel_avg_9_8_l100km() {
    sendFuelConsumptionAvgMessage(98);  // 9.8 L/100km
    uint8_t exp[] = {
        0x2E, 0x23, 0x0D, 0x02, 0x00, 0x62,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0x75
    };
    assertFrame(exp, sizeof(exp));
}

void test_fuel_avg_big_endian_value() {
    sendFuelConsumptionAvgMessage(300);  // 30.0 L/100km; exercise both value bytes
    uint8_t exp[] = {
        0x2E, 0x23, 0x0D, 0x02, 0x01, 0x2C,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xAA
    };
    assertFrame(exp, sizeof(exp));
}

void test_fuel_avg_zero_is_valid_trip() {
    sendFuelConsumptionAvgMessage(0);
    uint8_t exp[] = {
        0x2E, 0x23, 0x0D, 0x02, 0x00, 0x00,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xD7
    };
    assertFrame(exp, sizeof(exp));
}

void test_fuel_avg_unavailable_trip() {
    sendFuelConsumptionAvgMessage(0xFFFF);
    TEST_ASSERT_EQUAL(0, RadioSerial.captured.size());

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

static std::vector<uint8_t> findPayload(uint8_t command, uint8_t selector = 0xFF) {
    const auto& bytes = RadioSerial.captured;
    for (size_t i = 0; i < bytes.size();) {
        TEST_ASSERT_TRUE(i + 4 <= bytes.size());
        TEST_ASSERT_EQUAL_HEX8(0x2E, bytes[i]);
        const size_t length = bytes[i+2];
        TEST_ASSERT_TRUE(i + length + 4 <= bytes.size());
        uint8_t sum = 0;
        for (size_t j = i+1; j < i+length+4; ++j) sum += bytes[j];
        TEST_ASSERT_EQUAL_HEX8(0xFF, sum);
        if (bytes[i+1] == command &&
            (selector == 0xFF || (length && bytes[i+3] == selector))) {
            return {bytes.begin()+i+3, bytes.begin()+i+3+length};
        }
        i += length + 4;
    }
    return {};
}

static void freshBootSources() {
    canAuxData = {};
    canAuxData.duduReceiver = true;
    canAuxData.rpmValid = canAuxData.speedValid = canAuxData.odometerValid = true;
    canAuxData.dteValid = canAuxData.coolantValid = canAuxData.oilValid = true;
    canAuxData.rpmReceivedAt = canAuxData.speedReceivedAt = mockMillis;
    canAuxData.odometerReceivedAt = canAuxData.dteReceivedAt = mockMillis;
    canAuxData.coolantReceivedAt = canAuxData.oilReceivedAt = mockMillis;
    canAuxData.speedCentiKmh = 9321;
    canAuxData.oilTemperatureC = 95;
    canAuxData.oilEstimated = true;
    canAuxData.bootTripValid = true;
    canAuxData.bootTripDistanceDeciKm = 149;
    canAuxData.fuelConsumptionInstValid = canAuxData.fuelConsumptionAvgValid = true;
    canAuxData.fuelConsumptionDerived = true;
    engineRPM = 2709;
    vehicleSpeed = 93;
    currentOdo = 247498;
    dteValue = 153;
    coolantTemp = 85;
    tempExt = 28; // deliberately unvalidated: must not leak into UART
    averageSpeed = 561;
    elapsedTime = 957;
    fuelConsumptionInst = fuelConsumptionAvg = 81;
}

void test_scheduler_omits_unreceived_numeric_sources_and_unknown_temperatures() {
    canAuxData.fuelConsumptionDerived = true;
    engineRPM = 2709; currentOdo = 247498; dteValue = 153; tempExt = 28;
    mockMillis += 10000;
    processRadioUpdates();
    for (uint8_t command : {0x21, 0x22, 0x23, 0x28}) {
        TEST_ASSERT_TRUE(findPayload(command).empty());
    }
    for (uint8_t selector : {3, 4, 6, 9, 10}) {
        TEST_ASSERT_TRUE(findPayload(0x7D, selector).empty());
    }
}

void test_scheduler_publishes_boot_trip_in_receiver_units() {
    mockMillis += 10000;
    freshBootSources();
    processRadioUpdates();
    const auto speed = findPayload(0x7D, 3);
    TEST_ASSERT_EQUAL(6, speed.size());
    TEST_ASSERT_EQUAL_UINT16(9321, uint16_t(speed[1]) | uint16_t(speed[2]) << 8);
    TEST_ASSERT_EQUAL_UINT16(5610, uint16_t(speed[3]) | uint16_t(speed[4]) << 8);
    TEST_ASSERT_EQUAL_HEX8(0, speed[5]);
    const auto trip = findPayload(0x21);
    TEST_ASSERT_EQUAL(7, trip.size());
    TEST_ASSERT_EQUAL_UINT16(561, uint16_t(trip[0]) << 8 | trip[1]);
    TEST_ASSERT_EQUAL_UINT16(15, uint16_t(trip[2]) << 8 | trip[3]); // 957s ->15min
    TEST_ASSERT_EQUAL_UINT16(153, uint16_t(trip[4]) << 8 | trip[5]);
    TEST_ASSERT_EQUAL_HEX8(2, trip[6]);
    const auto odo = findPayload(0x7D, 4);
    TEST_ASSERT_EQUAL(12, odo.size());
    TEST_ASSERT_EQUAL_UINT32(247498, uint32_t(odo[1]) | uint32_t(odo[2]) << 8 | uint32_t(odo[3]) << 16);
    TEST_ASSERT_EQUAL_UINT16(149, uint16_t(odo[6]) | uint16_t(odo[7]) << 8);
    TEST_ASSERT_EQUAL_UINT16(149, uint16_t(odo[9]) | uint16_t(odo[10]) << 8);
    const auto temperatures = findPayload(0x7D, 9);
    TEST_ASSERT_EQUAL(4, temperatures.size());
    TEST_ASSERT_EQUAL_INT(85, int(temperatures[1]) - 40); // DUDU coolant
    TEST_ASSERT_EQUAL_INT(85, int(temperatures[2]) - 40); // FYT coolant
    TEST_ASSERT_EQUAL_INT(95, int(temperatures[3]) - 40); // DUDU provisional oil
    const auto average = findPayload(0x23);
    TEST_ASSERT_EQUAL(13, average.size());
    TEST_ASSERT_EQUAL_HEX8(2, average[0]);
    TEST_ASSERT_EQUAL_UINT16(81, uint16_t(average[1]) << 8 | average[2]);
    for (size_t i = 3; i < average.size(); ++i) TEST_ASSERT_EQUAL_HEX8(255, average[i]);
    TEST_ASSERT_TRUE(findPayload(0x28).empty()); // stored legacy28C is not valid
    TEST_ASSERT_TRUE(findPayload(0x7D, 6).empty()); // no fabricated seatbelt packet
}

void test_scheduler_stops_stale_measurements_and_rejects_unknown_oil() {
    mockMillis += 10000;
    freshBootSources();
    canAuxData.rpmReceivedAt = canAuxData.speedReceivedAt = mockMillis - 2000;
    canAuxData.odometerReceivedAt = canAuxData.dteReceivedAt = mockMillis - 2000;
    canAuxData.outsideValid = true;
    canAuxData.outsideReceivedAt = mockMillis - 2000;
    canAuxData.oilValid = false;
    processRadioUpdates();
    for (uint8_t selector : {3, 4, 9, 10}) TEST_ASSERT_TRUE(findPayload(0x7D, selector).empty());
    TEST_ASSERT_TRUE(findPayload(0x28).empty());
    const auto trip = findPayload(0x21);
    TEST_ASSERT_EQUAL(7, trip.size());
    TEST_ASSERT_EQUAL_HEX8(255, trip[4]); // DUDU ignores unavailable DTE
    TEST_ASSERT_EQUAL_HEX8(255, trip[5]);
}

void test_dudu_keeps_live_speed_and_range_after_boot_trip_gap() {
    mockMillis += 10000;
    freshBootSources();
    canAuxData.bootTripValid = false;
    canAuxData.fuelConsumptionAvgValid = false;
    processRadioUpdates();
    const auto speed = findPayload(0x7D, 3);
    TEST_ASSERT_EQUAL(6, speed.size());
    TEST_ASSERT_EQUAL_UINT16(9321, uint16_t(speed[1]) | uint16_t(speed[2]) << 8);
    TEST_ASSERT_EQUAL_HEX8(255, speed[3]);
    TEST_ASSERT_EQUAL_HEX8(255, speed[4]); // DUDU keeps old mean, never invents0
    TEST_ASSERT_EQUAL_HEX8(0, speed[5]);
    const auto trip = findPayload(0x21);
    TEST_ASSERT_EQUAL(7, trip.size());
    TEST_ASSERT_EQUAL_HEX8(255, trip[0]);
    TEST_ASSERT_EQUAL_HEX8(255, trip[1]);
    TEST_ASSERT_EQUAL_UINT16(153, uint16_t(trip[4]) << 8 | trip[5]);
    TEST_ASSERT_TRUE(findPayload(0x7D, 4).empty()); // no virtual incomplete trip
    TEST_ASSERT_TRUE(findPayload(0x23).empty()); // no false zero or FFFF mean
}

void test_classic_fyt_does_not_receive_dudu_only_ignore_markers() {
    mockMillis += 10000;
    freshBootSources();
    canAuxData.duduReceiver = false;
    canAuxData.bootTripValid = false;
    processRadioUpdates();
    TEST_ASSERT_TRUE(findPayload(0x7D, 3).empty());
    TEST_ASSERT_TRUE(findPayload(0x21).empty());
    TEST_ASSERT_TRUE(findPayload(0x7D, 4).empty());
}

void test_classic_fyt_preserves_coolant_only_packet_without_fake_oil() {
    mockMillis += 10000;
    freshBootSources();
    canAuxData.duduReceiver = false;
    canAuxData.oilValid = false;
    processRadioUpdates();
    const auto temperatures = findPayload(0x7D, 9);
    TEST_ASSERT_EQUAL(3, temperatures.size());
    TEST_ASSERT_EQUAL_HEX8(0, temperatures[1]);
    TEST_ASSERT_EQUAL_INT(85, int(temperatures[2]) - 40);
}

void test_elapsed_boot_time_does_not_wrap_at_65535_seconds() {
    mockMillis += 10000;
    freshBootSources();
    elapsedTime = 86400; //24h, longer than legacy U16 seconds
    processRadioUpdates();
    auto trip = findPayload(0x21);
    TEST_ASSERT_EQUAL(7, trip.size());
    TEST_ASSERT_EQUAL_UINT16(1440, uint16_t(trip[2]) << 8 | trip[3]);
    RadioSerial.clear();
    mockMillis += 10000;
    freshBootSources();
    elapsedTime = UINT32_MAX;
    processRadioUpdates();
    trip = findPayload(0x21);
    TEST_ASSERT_EQUAL(7, trip.size());
    TEST_ASSERT_EQUAL_UINT16(65534, uint16_t(trip[2]) << 8 | trip[3]);
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
    RUN_TEST(test_steering_saturates_before_narrowing);

    // RPM
    RUN_TEST(test_rpm_zero);
    RUN_TEST(test_rpm_1000);
    RUN_TEST(test_rpm_rejects_receiver_range_and_multiplication_wrap);

    // Speed
    RUN_TEST(test_speed_zero);
    RUN_TEST(test_speed_50kmh);
    RUN_TEST(test_speed_fractional_and_boot_average_with_explicit_unit);
    RUN_TEST(test_speed_rejects_values_outside_receiver_range);

    // Odometer
    RUN_TEST(test_odometer_244268km);
    RUN_TEST(test_odometer_zero);
    RUN_TEST(test_odometer_boot_trip_mirrored_with_deci_kilometres);
    RUN_TEST(test_odometer_rejects_narrowing_wrap);

    // Temperature
    RUN_TEST(test_temp_28c_without_false_climate_status);
    RUN_TEST(test_temp_minus10c);
    RUN_TEST(test_coolant_wire_85c);
    RUN_TEST(test_coolant_wire_complete_range_without_wrapping);
    RUN_TEST(test_coolant_wire_rejects_unrepresentable_values);
    RUN_TEST(test_coolant_scheduler_requires_received_fresh_source);

    // Trip info
    RUN_TEST(test_trip_305km_range);
    RUN_TEST(test_trip_zero);
    RUN_TEST(test_trip_elapsed_is_minutes_not_seconds);

    // Fuel consumption
    RUN_TEST(test_fuel_inst_6_5_l100km);
    RUN_TEST(test_fuel_inst_zero);
    RUN_TEST(test_fuel_instant_ffff_is_not_a_dudu_unavailable_value);
    RUN_TEST(test_fuel_avg_9_8_l100km);
    RUN_TEST(test_fuel_avg_big_endian_value);
    RUN_TEST(test_fuel_avg_zero_is_valid_trip);
    RUN_TEST(test_fuel_avg_unavailable_trip);

    // Lights
    RUN_TEST(test_lights_off);
    RUN_TEST(test_lights_headlights_only);
    RUN_TEST(test_lights_all_on);
    RUN_TEST(test_lights_pdf_example);

    // Checksum property
    RUN_TEST(test_checksum_property);
    RUN_TEST(test_scheduler_omits_unreceived_numeric_sources_and_unknown_temperatures);
    RUN_TEST(test_scheduler_publishes_boot_trip_in_receiver_units);
    RUN_TEST(test_scheduler_stops_stale_measurements_and_rejects_unknown_oil);
    RUN_TEST(test_dudu_keeps_live_speed_and_range_after_boot_trip_gap);
    RUN_TEST(test_classic_fyt_does_not_receive_dudu_only_ignore_markers);
    RUN_TEST(test_classic_fyt_preserves_coolant_only_packet_without_fake_oil);
    RUN_TEST(test_elapsed_boot_time_does_not_wrap_at_65535_seconds);

    return UNITY_END();
}
