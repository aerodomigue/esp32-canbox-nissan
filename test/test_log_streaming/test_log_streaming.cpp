#include <unity.h>
#include <regex>
#include <set>
#include <unistd.h>
#include "Arduino.h"
#include "ESP32-TWAI-CAN.hpp"
#include "LittleFS.h"
#include "CanConfigProcessor.h"
#include "ConfigManager.h"
#include "ConfigManager_mock.h"
#include "GlobalData.h"
#include "ObdCalibration.h"
#include "SerialCommand.h"
#include "UsbConsole.h"
#include "Update.h"
#include "MockDataGenerator.h"
#include "RadioSend.h"

extern void setup();
extern void loop();
extern CanConfigProcessor canProcessor;
extern ObdCalibration obdCalibration;
extern HardwareSerial RadioSerial;
static unsigned expectedRestarts = 0;

static CanFrame frame(uint32_t id, std::initializer_list<uint8_t> payload, bool extended = false, bool rtr = false) {
    CanFrame result = {};
    result.identifier = id;
    result.data_length_code = static_cast<uint8_t>(payload.size());
    std::copy(payload.begin(), payload.end(), result.data);
    result.extd = extended;
    result.rtr = rtr;
    return result;
}

static std::vector<std::string> lines(const std::string& output) {
    std::vector<std::string> result;
    size_t start = 0;
    for (size_t end = output.find('\n'); end != std::string::npos; end = output.find('\n', start)) {
        std::string line = output.substr(start, end - start);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        result.push_back(line);
        start = end + 1;
    }
    return result;
}

static size_t countPrefix(const std::string& output, const char* prefix) {
    size_t count = 0;
    for (const auto& line : lines(output)) if (line.rfind(prefix, 0) == 0) ++count;
    return count;
}

static bool hasLine(const std::string& output, const std::string& wanted) {
    for (const auto& line : lines(output)) if (line == wanted) return true;
    return false;
}

static void assertFrameUnchanged(const CanFrame& expected, const CanFrame& actual) {
    TEST_ASSERT_EQUAL_UINT32(expected.identifier, actual.identifier);
    TEST_ASSERT_EQUAL_UINT8(expected.data_length_code, actual.data_length_code);
    TEST_ASSERT_EQUAL(expected.extd, actual.extd);
    TEST_ASSERT_EQUAL(expected.rtr, actual.rtr);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected.data, actual.data, sizeof(expected.data));
}

static void assertRawLinesComplete(const std::string& output) {
    const std::regex raw("^(RX|ODB RAW_RX) 0x[0-9A-F]{3,8} \\[[0-8]\\]:(( [0-9A-F]{2})*) \\| t_ms=[0-9]+ ext=[01] rtr=[01]$");
    for (const auto& line : lines(output)) {
        if (line.rfind("RX", 0) == 0 || line.rfind("ODB RAW_RX", 0) == 0) {
            TEST_ASSERT_TRUE_MESSAGE(std::regex_match(line, raw), line.c_str());
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(output.empty() || output.back() == '\n', "Output ends inside a line");
}

static void flushConsole() {
    unsigned drains = 0;
    while (Console.queuedBytes() > 0 && drains++ < 2048) Console.drain();
    TEST_ASSERT_EQUAL_UINT32(0, Console.queuedBytes());
}

static void enableLoggingThroughLoop() {
    Serial.inject("LOG ON\n");
    loop();
    TEST_ASSERT_TRUE(isCanLogEnabled());
    TEST_ASSERT_TRUE(obdCalibration.enabled());
}

void setUp() {
    StreamingSimulation::onAdvance = {};
    StreamingSimulation::now = 0;
    StreamingSimulation::watchdogResets = 0;
    // Clean persistent command/OBD state with a ready simulated writer, then
    // reset the mock so production setup must configure its own USB timeout.
    Serial.connected = true;
    Serial.writeCapacity = 4096;
    Serial.txTimeoutMs = 0;
    Serial.scriptedLimits.clear();
    Serial.maxWrite = static_cast<size_t>(-1);
    Console.reset();
    ESP.restarts = 0;
    ESP.outputAtRestart.clear();
    expectedRestarts = 0;
    ESP32Can.reset();
    RadioSerial.clear();
    obdCalibration.setEnabled(false, 0);
    testProcessCommand("OTA ABORT");
    testProcessCommand("LOG OFF");
    Serial.reset();
    canProcessor = CanConfigProcessor{};
    mockReset();
    configSetVehicleFile("NissanJukeF15.json");
    LittleFS.basePath = "data";
    resetVehicleData();
    setup();
    TEST_ASSERT_FALSE(canProcessor.isMockMode());
    flushConsole();
    Serial.clearOutput();
}

void tearDown() {
    StreamingSimulation::onAdvance = {};
    TEST_ASSERT_EQUAL_UINT32(expectedRestarts, ESP.restarts);
    TEST_ASSERT_EQUAL_UINT32(0, Serial.unsafeWriteAttempts);
    TEST_ASSERT_EQUAL_UINT32(0, Serial.blockingWriteAttempts);
    for (const auto& write : Serial.writes) TEST_ASSERT_EQUAL_UINT32(0, write.timeoutMs);
}

void test_setup_uses_real_preset_and_can_queue() {
    TEST_ASSERT_EQUAL_STRING("Nissan Juke F15", canProcessor.getProfileName());
    TEST_ASSERT_TRUE(ESP32Can.started);
    TEST_ASSERT_EQUAL_UINT32(500, ESP32Can.convertArgument);
    TEST_ASSERT_EQUAL_UINT32(500, ESP32Can.speed);
    TEST_ASSERT_EQUAL_UINT32(128, ESP32Can.capacity);
    TEST_ASSERT_EQUAL_INT(21, ESP32Can.txPin);
    TEST_ASSERT_EQUAL_INT(20, ESP32Can.rxPin);
    TEST_ASSERT_EQUAL_UINT32(2000, StreamingSimulation::now);
    TEST_ASSERT_EQUAL_UINT32(4096, Serial.txBufferSize);
    TEST_ASSERT_EQUAL_UINT32(0, Serial.txTimeoutMs);
}

void test_no_obd_response_does_not_gate_continuous_rx() {
    enableLoggingThroughLoop();
    TEST_ASSERT_TRUE(obdCalibration.waiting());
    Serial.clearOutput();
    const CanFrame sample = frame(0x182, {0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF, 0x55, 0xAA});
    constexpr unsigned passes = 30;
    for (unsigned i = 0; i < passes; ++i) {
        for (unsigned n = 0; n < 32; ++n) TEST_ASSERT_TRUE(ESP32Can.enqueue(sample));
        loop();
        TEST_ASSERT_EQUAL_UINT32((i + 1) * 32, ESP32Can.reads.size());
        TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.inRxQueue());
        StreamingSimulation::advance(50);
    }
    TEST_ASSERT_EQUAL_UINT32(passes * 32, countPrefix(Serial.captured, "RX "));
    TEST_ASSERT_EQUAL_UINT32(passes * 32, canProcessor.getUnknownFrames());
    TEST_ASSERT_EQUAL_UINT32(0, countPrefix(Serial.captured, "ODB RX "));
    TEST_ASSERT_EQUAL_UINT32(3, countPrefix(Serial.captured, "ODB TIMEOUT "));
    TEST_ASSERT_TRUE(Serial.captured.find("status=no_supported_response retry_ms=5000") != std::string::npos);
    TEST_ASSERT_EQUAL_UINT32(3, ESP32Can.transmitted.size());
    for (const auto& request : ESP32Can.transmitted) {
        TEST_ASSERT_EQUAL_HEX32(0x7DF, request.identifier);
        TEST_ASSERT_EQUAL_HEX8(0x01, request.data[1]);
        TEST_ASSERT_EQUAL_HEX8(0x00, request.data[2]);
    }
    for (const auto timeout : ESP32Can.readTimeouts) TEST_ASSERT_EQUAL_UINT32(0, timeout);
    for (const auto timeout : ESP32Can.writeTimeouts) TEST_ASSERT_EQUAL_UINT32(0, timeout);
    for (const auto& read : ESP32Can.reads) assertFrameUnchanged(sample, read.frame);
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.missed);
    assertRawLinesComplete(Serial.captured);
}

void test_loop_drains_only_32_frames_then_continues_while_obd_waits() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    for (unsigned n = 0; n < 40; ++n) TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {static_cast<uint8_t>(n)})));
    loop();
    TEST_ASSERT_TRUE(obdCalibration.waiting());
    TEST_ASSERT_EQUAL_UINT32(32, ESP32Can.reads.size());
    TEST_ASSERT_EQUAL_UINT32(8, ESP32Can.inRxQueue());
    loop();
    TEST_ASSERT_EQUAL_UINT32(40, ESP32Can.reads.size());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.inRxQueue());
    TEST_ASSERT_EQUAL_UINT32(40, countPrefix(Serial.captured, "RX "));
}

void test_raw_can_keeps_payload_flags_and_unconfigured_frames() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    const std::vector<CanFrame> samples = {
        frame(0x180, {0x04, 0xFB, 0xAA, 0x55, 0xCC, 0x33, 0xFF, 0x00}),
        frame(0x182, {0x00, 0xFF, 0x7F, 0x80, 0x12, 0xAB, 0xCD, 0xEF}),
        frame(0x1ABCDE27, {0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10}, true),
        frame(0x180, {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}, false, true),
        frame(0x7E8, {0x03, 0x41, 0x0D, 0x64, 0x00, 0x00, 0x00, 0x00}),
        frame(0x7E8, {0xDE, 0xAD}, true),
        frame(0x000, {})
    };
    for (const auto& sample : samples) TEST_ASSERT_TRUE(ESP32Can.enqueue(sample));
    loop();
    TEST_ASSERT_EQUAL_UINT32(samples.size(), ESP32Can.reads.size());
    for (size_t i = 0; i < samples.size(); ++i) assertFrameUnchanged(samples[i], ESP32Can.reads[i].frame);
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x180 [8]: 04 FB AA 55 CC 33 FF 00 | t_ms=2000 ext=0 rtr=0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x182 [8]: 00 FF 7F 80 12 AB CD EF | t_ms=2000 ext=0 rtr=0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x1ABCDE27 [8]: FE DC BA 98 76 54 32 10 | t_ms=2000 ext=1 rtr=0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x180 [8]: | t_ms=2000 ext=0 rtr=1"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "ODB RAW_RX 0x7E8 [8]: 03 41 0D 64 00 00 00 00 | t_ms=2000 ext=0 rtr=0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x7E8 [2]: DE AD | t_ms=2000 ext=1 rtr=0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x000 [0]: | t_ms=2000 ext=0 rtr=0"));
    TEST_ASSERT_EQUAL_UINT16(159, engineRPM);  // Corrected /8; RTR/extended never overwrite it.
    TEST_ASSERT_EQUAL_UINT32(1, canProcessor.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(3, canProcessor.getUnknownFrames());
    assertRawLinesComplete(Serial.captured);
}

void test_obd_reference_sys_data_and_partial_command_keep_complete_lines() {
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x180, {0x04, 0xFB, 0, 0, 0, 0, 0, 0})));
    enableLoggingThroughLoop();
    TEST_ASSERT_EQUAL_UINT16(159, engineRPM);
    StreamingSimulation::advance(1);
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x7E8, {0x06, 0x41, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00})));
    loop();
    StreamingSimulation::advance(499);
    loop();
    TEST_ASSERT_EQUAL_HEX8(0x0C, ESP32Can.transmitted.back().data[2]);
    StreamingSimulation::advance(1);
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x7E8, {0x04, 0x41, 0x0C, 0x0A, 0x0C, 0x00, 0x00, 0x00})));
    loop();
    TEST_ASSERT_EQUAL_UINT16(159, engineRPM);  // 643 rpm OBD reference is kept separate.
    const std::regex reference("^ODB RX t_ms=2501 seq=[0-9]+ request_id=0x7E0 ecu=0x7E8 pid=0x0C name=rpm value=643\\.000 unit=rpm latency_ms=1$");
    bool matched = false;
    for (const auto& line : lines(Serial.captured)) matched |= std::regex_match(line, reference);
    TEST_ASSERT_TRUE(matched);
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "ODB RAW_RX 0x7E8 [8]: 04 41 0C 0A 0C 00 00 00 | t_ms=2501 ext=0 rtr=0"));
    Serial.clearOutput();
    Serial.inject("SYS DA");
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {0x11, 0x22})));
    loop();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x182 [2]: 11 22 | t_ms=2501 ext=0 rtr=0"));
    TEST_ASSERT_TRUE(Serial.captured.find("SYS DA") == std::string::npos);
    Serial.inject("TA\n");
    loop();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "=== Live Vehicle Data ==="));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RPM:      159"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "Speed:    0 km/h"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "========================="));
    assertRawLinesComplete(Serial.captured);
}

static unsigned radioCommandCount(uint8_t command) {
    unsigned count = 0;
    const auto& bytes = RadioSerial.captured;
    for (size_t offset = 0; offset < bytes.size();) {
        TEST_ASSERT_TRUE(offset + 4 <= bytes.size());
        TEST_ASSERT_EQUAL_HEX8(0x2E, bytes[offset]);
        size_t length = bytes[offset + 2] + 4;
        TEST_ASSERT_TRUE(offset + length <= bytes.size());
        if (bytes[offset + 1] == command) ++count;
        offset += length;
    }
    return count;
}

void test_sys_data_distinguishes_unknown_from_observed_zero() {
    Serial.inject("SYS DATA\n");
    loop();
    flushConsole();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "Handbrake: unknown"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "Brake: unknown"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "FuelRaw: unknown"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "FuelValid: 0"));

    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x5C5, {0x40, 0x03, 0xC4, 0xA1, 0, 0x0C, 0, 0x7F})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x180, {0x15, 0xE0, 0x31, 0xF3, 0x11, 0, 0x30, 0x14})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x551, {0x67, 0, 0, 0x80, 0xFF, 2, 0x80, 0xFF})));
    loop();
    Serial.clearOutput();
    Serial.inject("SYS DATA\n");
    loop();
    flushConsole();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "Handbrake: 0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "Brake: 0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "FuelRaw: 0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "FuelConsValid: INST=0 AVG=0"));
    assertRawLinesComplete(Serial.captured);
}

void test_juke_raw_fuel_and_brakes_keep_logs_and_do_not_transmit_consumption() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x5C5, {0x44, 0x03, 0xC4, 0xA1, 0, 0x0C, 0, 0x7F})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x180, {0x15, 0xE0, 0x31, 0xF3, 0x11, 0, 0x30, 0x15})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x551, {0x66, 0x34, 0, 0x80, 0xFF, 2, 0x80, 0xFF})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x580, {0, 0, 0x80, 6, 111, 0, 0, 0})));
    loop();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x551 [8]: 66 34 00 80 FF 02 80 FF | t_ms=2000 ext=0 rtr=0"));
    TEST_ASSERT_EQUAL_UINT32(4, countPrefix(Serial.captured, "RX "));
    Serial.inject("SYS DATA\n");
    loop();
    flushConsole();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "Handbrake: 1"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "Brake: 1"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "FuelRaw: 52"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "FuelValid: 0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "FuelConsValid: INST=0 AVG=0"));
    TEST_ASSERT_EQUAL_UINT16(0, fuelConsumptionInst);
    TEST_ASSERT_EQUAL_UINT16(0, fuelConsumptionAvg);
    RadioSerial.clear();
    StreamingSimulation::advance(6000);
    loop();
    TEST_ASSERT_EQUAL_UINT32(0, radioCommandCount(0x22));
    TEST_ASSERT_EQUAL_UINT32(0, radioCommandCount(0x23));
    TEST_ASSERT_GREATER_THAN_UINT32(0, radioCommandCount(0x7D));
    assertRawLinesComplete(Serial.captured);
}

void test_mock_consumption_is_available_and_profile_reload_clears_it() {
    MockDataGenerator generator;
    generator.begin();
    TEST_ASSERT_TRUE(canAuxData.fuelLevelValid);
    TEST_ASSERT_TRUE(canAuxData.fuelConsumptionInstValid);
    TEST_ASSERT_TRUE(canAuxData.fuelConsumptionAvgValid);
    processRadioUpdates();  // Normalize persistent timers after the test clock reset.
    RadioSerial.clear();
    StreamingSimulation::advance(6000);
    processRadioUpdates();
    TEST_ASSERT_EQUAL_UINT32(1, radioCommandCount(0x22));
    TEST_ASSERT_EQUAL_UINT32(1, radioCommandCount(0x23));
    canAuxData.handbrakeValid = true;
    canAuxData.brakePedalValid = true;
    canAuxData.fuelCounterValid = true;
    TEST_ASSERT_TRUE(canProcessor.loadFromJson("/NissanJukeF15.json"));
    TEST_ASSERT_FALSE(canAuxData.handbrakeValid);
    TEST_ASSERT_FALSE(canAuxData.brakePedalValid);
    TEST_ASSERT_FALSE(canAuxData.fuelCounterValid);
    TEST_ASSERT_FALSE(canAuxData.fuelLevelValid);
    TEST_ASSERT_FALSE(canAuxData.fuelConsumptionInstValid);
    TEST_ASSERT_FALSE(canAuxData.fuelConsumptionAvgValid);
    TEST_ASSERT_EQUAL_UINT8(0, fuelLevel);
    TEST_ASSERT_EQUAL_UINT16(0, fuelConsumptionInst);
    TEST_ASSERT_EQUAL_UINT16(0, fuelConsumptionAvg);
    RadioSerial.clear();
    StreamingSimulation::advance(6000);
    processRadioUpdates();
    TEST_ASSERT_EQUAL_UINT32(0, radioCommandCount(0x22));
    TEST_ASSERT_EQUAL_UINT32(0, radioCommandCount(0x23));
}

static void assertIndicatorOutputs(bool left, bool right) {
    // Read the public dashboard response after the CAN acquisition/radio loop.
    Serial.clearOutput();
    Serial.inject("SYS DATA\n");
    loop();
    flushConsole();
    const std::string expected = std::string("Lights:   H=0 P=0 HB=0 L=") +
        (left ? "1" : "0") + " R=" + (right ? "1" : "0");
    TEST_ASSERT_TRUE_MESSAGE(hasLine(Serial.captured, expected), Serial.captured.c_str());

    // Compare with the actual UART lights frame, not just the dashboard globals.
    bool found = false;
    uint8_t lights = 0;
    for (size_t offset = 0; offset < RadioSerial.captured.size();) {
        TEST_ASSERT_TRUE(offset + 4 <= RadioSerial.captured.size());
        TEST_ASSERT_EQUAL_HEX8(0x2E, RadioSerial.captured[offset]);
        const size_t length = RadioSerial.captured[offset + 2];
        TEST_ASSERT_TRUE(offset + length + 4 <= RadioSerial.captured.size());
        if (RadioSerial.captured[offset + 1] == 0x7D && length == 2 &&
            RadioSerial.captured[offset + 3] == 0x01) {
            found = true;
            lights = RadioSerial.captured[offset + 4];
        }
        offset += length + 4;
    }
    TEST_ASSERT_TRUE_MESSAGE(found, "No UART lights frame emitted");
    TEST_ASSERT_EQUAL_HEX8((left ? 0x10 : 0) | (right ? 0x08 : 0), lights);
}

void test_sys_data_indicators_match_left_right_and_hazard_can_frames() {
    configSetIndicatorTimeout(500);
    // Advance past the previous signal's timeout before testing each state.
    // Nonzero simulated time makes a missing timestamp update observable.
    for (const uint8_t bits : {0x20, 0x40, 0x60}) {
        StreamingSimulation::advance(1000);
        RadioSerial.clear();
        TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x60D, {0, bits, 0})));
        loop();
        assertIndicatorOutputs((bits & 0x20) != 0, (bits & 0x40) != 0);
    }
}

void test_sys_data_indicators_expire_at_configured_timeout_without_new_can() {
    configSetIndicatorTimeout(800);
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x60D, {0, 0x60, 0})));
    loop();
    assertIndicatorOutputs(true, true);
    StreamingSimulation::advance(799);
    loop();
    assertIndicatorOutputs(true, true);
    StreamingSimulation::advance(1);
    loop();
    assertIndicatorOutputs(false, false);
}

void test_sys_data_indicator_refresh_and_zero_frame_keep_radio_hold_time() {
    configSetIndicatorTimeout(500);
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x60D, {0, 0x20, 0})));
    loop();
    assertIndicatorOutputs(true, false);
    StreamingSimulation::advance(400);
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x60D, {0, 0x20, 0})));
    loop();
    StreamingSimulation::advance(400);
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x60D, {0, 0, 0})));
    loop();
    // A zero input does not cancel the existing radio hold immediately.
    assertIndicatorOutputs(true, false);
    StreamingSimulation::advance(100);
    loop();
    assertIndicatorOutputs(false, false);
}

void test_short_usb_write_resumes_exactly_without_truncation_or_duplicates() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    Serial.scriptedLimits = {8, 0};
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {0xDE, 0xAD, 0xBE, 0xEF})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x183, {0x12, 0x34})));
    loop();
    TEST_ASSERT_EQUAL_STRING("RX 0x182", Serial.captured.c_str());
    TEST_ASSERT_GREATER_THAN_UINT32(0, Console.queuedBytes());
    TEST_ASSERT_GREATER_THAN_UINT32(0, Console.shortWrites());
    flushConsole();
    const std::string expected = "RX 0x182 [4]: DE AD BE EF | t_ms=2000 ext=0 rtr=0\r\n"
                                 "RX 0x183 [2]: 12 34 | t_ms=2000 ext=0 rtr=0\r\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), Serial.captured.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, ESP32Can.reads.size());
    TEST_ASSERT_EQUAL_UINT32(2, canProcessor.getUnknownFrames());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.rxMissedCounter());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.txFailedCounter());
    TEST_ASSERT_EQUAL_UINT32(0, Console.droppedRawLines());
    assertRawLinesComplete(Serial.captured);
}

void test_usb_stop_before_crlf_preserves_separate_rx_and_odb_lines() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    const std::string first = "RX 0x182 [2]: DE AD | t_ms=2000 ext=0 rtr=0";
    // Stop exactly before CRLF in a FIFO chunk; the next drain must retain it.
    Serial.scriptedLimits = {first.size(), 0};
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {0xDE, 0xAD})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x7E8, {0x03, 0x41, 0x0D, 0x64, 0, 0, 0, 0})));
    loop();
    TEST_ASSERT_EQUAL_STRING(first.c_str(), Serial.captured.c_str());
    flushConsole();
    const std::string expected = first + "\r\nODB RAW_RX 0x7E8 [8]: 03 41 0D 64 00 00 00 00 | t_ms=2000 ext=0 rtr=0\r\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), Serial.captured.c_str());
    TEST_ASSERT_EQUAL_UINT32(2, lines(Serial.captured).size());
    TEST_ASSERT_EQUAL_UINT32(1, countPrefix(Serial.captured, "ODB RAW_RX "));
    TEST_ASSERT_EQUAL_UINT32(2, ESP32Can.reads.size());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.rxMissedCounter());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.txFailedCounter());
    assertRawLinesComplete(Serial.captured);
}

void test_usb_unavailable_for_500ms_keeps_can_acquisition_and_all_500_lines() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    const uint32_t startedAt = StreamingSimulation::now;
    const size_t previousReads = ESP32Can.reads.size();
    Serial.connected = false;
    Serial.writeCapacity = 0;
    StreamingSimulation::onAdvance = [](uint32_t, uint32_t elapsed) {
        // Synthetic 1000 frames/s input while the USB consumer is unavailable.
        // One loop per virtual ms; no hardware scheduling/timing claim is made.
        for (uint32_t i = 0; i < elapsed; ++i) {
            ESP32Can.enqueue(frame(0x182, {static_cast<uint8_t>(StreamingSimulation::now >> 8),
                                         static_cast<uint8_t>(StreamingSimulation::now), 0xDE, 0xAD, 0xBE, 0xEF, 0x55, 0xAA}));
        }
    };
    for (unsigned i = 0; i < 500; ++i) {
        StreamingSimulation::advance(1);
        loop();
        TEST_ASSERT_EQUAL_UINT32(i + 1, ESP32Can.reads.size() - previousReads);
        TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.inRxQueue());
    }
    StreamingSimulation::onAdvance = {};
    TEST_ASSERT_EQUAL_UINT32(500, StreamingSimulation::now - startedAt);
    TEST_ASSERT_EQUAL_UINT32(500, ESP32Can.reads.size() - previousReads);
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.rxMissedCounter());
    TEST_ASSERT_EQUAL_UINT32(0, Console.droppedRawLines());
    TEST_ASSERT_EQUAL_UINT32(0, Console.droppedDiagnosticLines());
    TEST_ASSERT_GREATER_THAN_UINT32(0, Console.queuedBytes());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(65536, Console.queuedBytes());
    TEST_ASSERT_EQUAL_UINT32(0, Serial.writes.size());
    TEST_ASSERT_EQUAL_UINT32(startedAt + 1, ESP32Can.reads[previousReads].at);
    TEST_ASSERT_EQUAL_UINT32(startedAt + 500, ESP32Can.reads.back().at);
    Serial.connected = true;
    Serial.writeCapacity = 4096;
    flushConsole();
    TEST_ASSERT_EQUAL_UINT32(500, countPrefix(Serial.captured, "RX "));
    assertRawLinesComplete(Serial.captured);
    std::set<std::string> unique;
    for (const auto& line : lines(Serial.captured)) if (line.rfind("RX ", 0) == 0) unique.insert(line);
    TEST_ASSERT_EQUAL_UINT32(500, unique.size());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.rxMissedCounter());
}

void test_fifo_pressure_drops_whole_raw_and_diagnostic_lines_preserving_commands() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    Serial.connected = false;
    Serial.writeCapacity = 0;
    constexpr unsigned total = 1280;
    for (unsigned base = 0; base < total; base += 32) {
        for (unsigned i = base; i < base + 32; ++i) {
            TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {static_cast<uint8_t>(i >> 8), static_cast<uint8_t>(i),
                                                         0xDE, 0xAD, 0xBE, 0xEF, 0x55, 0xAA})));
        }
        loop();
    }
    const uint32_t rawDrops = Console.droppedRawLines();
    TEST_ASSERT_GREATER_THAN_UINT32(0, rawDrops);
    TEST_ASSERT_EQUAL_UINT32(total, ESP32Can.reads.size());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.rxMissedCounter());
    for (unsigned i = 0; i < 32; ++i) {
        Console.logLine("ODB TEST diagnostic reserve pressure with a complete deterministic line", true);
    }
    TEST_ASSERT_GREATER_THAN_UINT32(0, Console.droppedDiagnosticLines());
    TEST_ASSERT_TRUE(Console.canProcessCommand());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(65536 - 12288, Console.queuedBytes());
    Serial.inject("SYS DATA\nLOG OFF\n");
    loop();  // The first command uses the command reserve even with raw FIFO saturation.
    TEST_ASSERT_TRUE(isCanLogEnabled());
    TEST_ASSERT_GREATER_THAN_INT(0, Serial.available());
    Serial.connected = true;
    Serial.writeCapacity = 4096;
    flushConsole();
    loop();
    flushConsole();
    TEST_ASSERT_FALSE(isCanLogEnabled());
    TEST_ASSERT_FALSE(obdCalibration.enabled());
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "=== Live Vehicle Data ==="));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "CAN logging disabled"));
    TEST_ASSERT_EQUAL_UINT32(total - rawDrops, countPrefix(Serial.captured, "RX "));
    std::set<std::string> unique;
    for (const auto& line : lines(Serial.captured)) if (line.rfind("RX ", 0) == 0) unique.insert(line);
    TEST_ASSERT_EQUAL_UINT32(total - rawDrops, unique.size());
    assertRawLinesComplete(Serial.captured);
}

void test_usb_drain_has_bounded_work_and_stops_immediately_after_zero_write() {
    const std::string text(100, 'A');
    for (unsigned i = 0; i < 200; ++i) Console.logLine(text.c_str());
    const size_t queued = Console.queuedBytes();
    Serial.writeCapacity = 32768;
    Console.drain();
    TEST_ASSERT_EQUAL_UINT32(4096, Serial.captured.size());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(8, Serial.writes.size());
    TEST_ASSERT_EQUAL_UINT32(queued - 4096, Console.queuedBytes());
    Serial.clearOutput();
    Serial.writeCapacity = 4096;
    Serial.maxWrite = 1;
    const size_t beforePartial = Console.queuedBytes();
    Console.drain();
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(8, Serial.writes.size());
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(4096, Serial.captured.size());
    TEST_ASSERT_EQUAL_UINT32(beforePartial - Serial.captured.size(), Console.queuedBytes());
    Serial.maxWrite = static_cast<size_t>(-1);
    Serial.clearOutput();
    Serial.scriptedLimits = {0};
    const size_t beforeZero = Console.queuedBytes();
    Console.drain();
    TEST_ASSERT_EQUAL_UINT32(1, Serial.writes.size());
    TEST_ASSERT_EQUAL_UINT32(beforeZero, Console.queuedBytes());
    Serial.clearOutput();
    Serial.writeCapacity = 7;
    Console.drain();
    TEST_ASSERT_LESS_OR_EQUAL_UINT32(8, Serial.writes.size());
    for (const auto& write : Serial.writes) TEST_ASSERT_LESS_OR_EQUAL_UINT32(7, write.attempted.size());
    Serial.writeCapacity = 4096;
    flushConsole();
}

void test_disconnected_or_zero_capacity_usb_is_never_written() {
    Console.println("queued while USB unavailable");
    const size_t queued = Console.queuedBytes();
    Serial.connected = false;
    Serial.writeCapacity = 4096;
    Console.drain();
    TEST_ASSERT_EQUAL_UINT32(0, Serial.writes.size());
    Serial.connected = true;
    Serial.writeCapacity = 0;
    Console.drain();
    TEST_ASSERT_EQUAL_UINT32(0, Serial.writes.size());
    Serial.writeCapacity = -1;
    Console.drain();
    TEST_ASSERT_EQUAL_UINT32(0, Serial.writes.size());
    TEST_ASSERT_EQUAL_UINT32(queued, Console.queuedBytes());
    Serial.writeCapacity = 4096;
    flushConsole();
    TEST_ASSERT_EQUAL_STRING("queued while USB unavailable\r\n", Serial.captured.c_str());
}

void test_serial_processes_one_command_per_loop_without_stopping_can() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    Serial.inject("SYS DATA\nLOG OFF\n");
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x180, {0x04, 0xFB, 0, 0, 0, 0, 0, 0})));
    loop();
    TEST_ASSERT_TRUE(isCanLogEnabled());
    TEST_ASSERT_GREATER_THAN_INT(0, Serial.available());
    TEST_ASSERT_EQUAL_UINT16(159, engineRPM);
    loop();
    TEST_ASSERT_FALSE(isCanLogEnabled());
    TEST_ASSERT_FALSE(obdCalibration.enabled());
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "=== Live Vehicle Data ==="));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "CAN logging disabled"));
    assertRawLinesComplete(Serial.captured);
}

void test_ota_ack_follows_pending_rx_fragment_without_mixing_lines() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    Serial.scriptedLimits = {8, 0};
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {0xDE, 0xAD, 0xBE, 0xEF})));
    loop();
    TEST_ASSERT_EQUAL_STRING("RX 0x182", Serial.captured.c_str());
    Serial.writeCapacity = 0;
    Serial.inject("OTA START 4\n");
    loop();
    TEST_ASSERT_TRUE(isOtaInProgress());
    Serial.inject("OTA DATA AQIDBA==\n");
    loop();
    const uint8_t expectedBytes[] = {1, 2, 3, 4};
    TEST_ASSERT_EQUAL_UINT32(4, Update.received.size());
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expectedBytes, Update.received.data(), 4);
    Serial.writeCapacity = 4096;
    flushConsole();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x182 [4]: DE AD BE EF | t_ms=2000 ext=0 rtr=0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "OTA starting: expecting 4 bytes"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "OK READY"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "OK 4/4 (100%)"));
    const size_t rawEnd = Serial.captured.find("rtr=0\r\n");
    const size_t ready = Serial.captured.find("OK READY");
    const size_t ack = Serial.captured.find("OK 4/4 (100%)");
    TEST_ASSERT_TRUE(rawEnd < ready && ready < ack);
    TEST_ASSERT_EQUAL_UINT32(1, countPrefix(Serial.captured, "RX "));
    assertRawLinesComplete(Serial.captured);
    Serial.inject("OTA ABORT\n");
    loop();
    flushConsole();
    TEST_ASSERT_FALSE(isOtaInProgress());
}

void test_oversized_command_response_rolls_back_to_one_complete_error() {
    const char* previous = "RX 0x182 [1]: AB | t_ms=2000 ext=0 rtr=0";
    Console.logLine(previous);
    Console.beginCommand();
    Console.println("This partial response must be rolled back");
    const std::vector<uint8_t> oversized(UsbConsole::CAPACITY, 'X');
    Console.write(oversized.data(), oversized.size());
    Console.println("No response suffix may escape the rollback");
    Console.endCommand();
    flushConsole();
    const std::string expected = std::string(previous) + "\r\nERROR: Response exceeds USB buffer or allocation failed\r\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), Serial.captured.c_str());
    TEST_ASSERT_EQUAL_UINT32(1, Console.rejectedResponses());
    TEST_ASSERT_EQUAL_UINT32(0, Console.droppedRawLines());
    assertRawLinesComplete(Serial.captured);
}

void test_oversized_can_get_keeps_echo_separate_from_error() {
    // Exercise the real command parser, including echo while LOG is OFF.
    char directory[] = ".pio/stream-response-XXXXXX";
    TEST_ASSERT_NOT_NULL(mkdtemp(directory));
    const std::string path = std::string(directory) + "/large.json";
    FILE* fixture = fopen(path.c_str(), "wb");
    TEST_ASSERT_NOT_NULL(fixture);
    const std::string content(UsbConsole::CAPACITY, 'X');
    const size_t written = fwrite(content.data(), 1, content.size(), fixture);
    fclose(fixture);
    LittleFS.basePath = directory;
    configSetVehicleFile("large.json");
    Serial.inject("CAN GET\n");
    loop();
    flushConsole();
    LittleFS.basePath = "data";
    remove(path.c_str());
    rmdir(directory);

    TEST_ASSERT_EQUAL_UINT32(content.size(), written);
    TEST_ASSERT_EQUAL_STRING(
        "CAN GET\r\nERROR: Response exceeds USB buffer or allocation failed\r\n",
        Serial.captured.c_str());
    TEST_ASSERT_EQUAL_UINT32(1, Console.rejectedResponses());
}

void test_complete_pending_command_is_retained_until_response_reserve_is_available() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    Serial.connected = false;
    Serial.writeCapacity = 0;
    const std::string existing(UsbConsole::CAPACITY - UsbConsole::COMMAND_RESERVE + 100, 'P');
    Console.print(existing.c_str());
    Console.println();
    TEST_ASSERT_FALSE(Console.canProcessCommand());
    Serial.inject("LOG OFF\n");
    loop();
    TEST_ASSERT_EQUAL_UINT32(0, Serial.available());
    TEST_ASSERT_TRUE(isCanLogEnabled());
    TEST_ASSERT_TRUE(obdCalibration.enabled());
    loop();
    TEST_ASSERT_TRUE(isCanLogEnabled());
    Serial.connected = true;
    Serial.writeCapacity = 4096;
    loop();
    flushConsole();
    TEST_ASSERT_FALSE(isCanLogEnabled());
    TEST_ASSERT_FALSE(obdCalibration.enabled());
    TEST_ASSERT_EQUAL_UINT32(1, countPrefix(Serial.captured, "CAN logging disabled"));
    TEST_ASSERT_EQUAL_UINT32(0, Console.rejectedResponses());
}

void test_ota_end_ack_and_completion_reach_usb_before_simulated_restart() {
    Serial.inject("OTA START 4\n");
    loop();
    TEST_ASSERT_TRUE(isOtaInProgress());
    Serial.inject("OTA DATA AQIDBA==\n");
    loop();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "OK 4/4 (100%)"));
    Serial.inject("OTA END\n");
    loop();
    expectedRestarts = 1;
    TEST_ASSERT_EQUAL_UINT32(1, ESP.restarts);
    TEST_ASSERT_FALSE(isOtaInProgress());
    TEST_ASSERT_TRUE(hasLine(ESP.outputAtRestart, "OK READY"));
    TEST_ASSERT_TRUE(hasLine(ESP.outputAtRestart, "OK 4/4 (100%)"));
    TEST_ASSERT_TRUE(hasLine(ESP.outputAtRestart, "Firmware updated successfully!"));
    TEST_ASSERT_TRUE(hasLine(ESP.outputAtRestart, "Rebooting in 2 seconds..."));
    TEST_ASSERT_TRUE(ESP.outputAtRestart.back() == '\n');
}

void test_fifo_wraparound_preserves_partial_suffix_before_new_response() {
    // Advance both ring cursors near the end, then queue a record across it.
    const std::string positioning(50000, 'P');
    Console.print(positioning.c_str());
    flushConsole();
    Serial.clearOutput();
    const std::string first(16000, 'A');
    const std::string second(2000, 'B');
    Console.println(first.c_str());
    Serial.scriptedLimits = {3, 0};
    Console.drain();
    TEST_ASSERT_EQUAL_STRING("AAA", Serial.captured.c_str());
    Console.println(second.c_str());
    TEST_ASSERT_EQUAL_UINT32(first.size() + second.size() + 4 - 3, Console.queuedBytes());
    flushConsole();
    const std::string expected = first + "\r\n" + second + "\r\n";
    TEST_ASSERT_EQUAL_STRING(expected.c_str(), Serial.captured.c_str());
    TEST_ASSERT_EQUAL_UINT32(0, Console.rejectedResponses());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_setup_uses_real_preset_and_can_queue);
    RUN_TEST(test_no_obd_response_does_not_gate_continuous_rx);
    RUN_TEST(test_loop_drains_only_32_frames_then_continues_while_obd_waits);
    RUN_TEST(test_raw_can_keeps_payload_flags_and_unconfigured_frames);
    RUN_TEST(test_obd_reference_sys_data_and_partial_command_keep_complete_lines);
    RUN_TEST(test_sys_data_distinguishes_unknown_from_observed_zero);
    RUN_TEST(test_juke_raw_fuel_and_brakes_keep_logs_and_do_not_transmit_consumption);
    RUN_TEST(test_mock_consumption_is_available_and_profile_reload_clears_it);
    RUN_TEST(test_sys_data_indicators_match_left_right_and_hazard_can_frames);
    RUN_TEST(test_sys_data_indicators_expire_at_configured_timeout_without_new_can);
    RUN_TEST(test_sys_data_indicator_refresh_and_zero_frame_keep_radio_hold_time);
    RUN_TEST(test_short_usb_write_resumes_exactly_without_truncation_or_duplicates);
    RUN_TEST(test_usb_stop_before_crlf_preserves_separate_rx_and_odb_lines);
    RUN_TEST(test_usb_unavailable_for_500ms_keeps_can_acquisition_and_all_500_lines);
    RUN_TEST(test_fifo_pressure_drops_whole_raw_and_diagnostic_lines_preserving_commands);
    RUN_TEST(test_usb_drain_has_bounded_work_and_stops_immediately_after_zero_write);
    RUN_TEST(test_disconnected_or_zero_capacity_usb_is_never_written);
    RUN_TEST(test_serial_processes_one_command_per_loop_without_stopping_can);
    RUN_TEST(test_ota_ack_follows_pending_rx_fragment_without_mixing_lines);
    RUN_TEST(test_oversized_command_response_rolls_back_to_one_complete_error);
    RUN_TEST(test_oversized_can_get_keeps_echo_separate_from_error);
    RUN_TEST(test_complete_pending_command_is_retained_until_response_reserve_is_available);
    RUN_TEST(test_ota_end_ack_and_completion_reach_usb_before_simulated_restart);
    RUN_TEST(test_fifo_wraparound_preserves_partial_suffix_before_new_response);
    return UNITY_END();
}
