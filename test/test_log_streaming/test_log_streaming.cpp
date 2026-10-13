#include <unity.h>
#include <regex>
#include "Arduino.h"
#include "ESP32-TWAI-CAN.hpp"
#include "LittleFS.h"
#include "CanConfigProcessor.h"
#include "ConfigManager.h"
#include "ConfigManager_mock.h"
#include "GlobalData.h"
#include "ObdCalibration.h"
#include "SerialCommand.h"

extern void setup();
extern void loop();
extern CanConfigProcessor canProcessor;
extern ObdCalibration obdCalibration;
extern HardwareSerial RadioSerial;

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
    Serial.reset();
    ESP.restarts = 0;
    ESP32Can.reset();
    RadioSerial.clear();
    obdCalibration.setEnabled(false, 0);
    testProcessCommand("LOG OFF");
    canProcessor = CanConfigProcessor{};
    mockReset();
    configSetVehicleFile("NissanJukeF15.json");
    LittleFS.basePath = "data";
    resetVehicleData();
    setup();
    TEST_ASSERT_FALSE(canProcessor.isMockMode());
    Serial.clearOutput();
}

void tearDown() {
    StreamingSimulation::onAdvance = {};
    TEST_ASSERT_EQUAL_UINT32(0, ESP.restarts);
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
    TEST_ASSERT_EQUAL_UINT16(182, engineRPM);  // Initial preset /7; RTR/extended never overwrite it.
    TEST_ASSERT_EQUAL_UINT32(1, canProcessor.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(3, canProcessor.getUnknownFrames());
    assertRawLinesComplete(Serial.captured);
}

void test_obd_reference_sys_data_and_partial_command_keep_complete_lines() {
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x180, {0x04, 0xFB, 0, 0, 0, 0, 0, 0})));
    enableLoggingThroughLoop();
    TEST_ASSERT_EQUAL_UINT16(182, engineRPM);
    StreamingSimulation::advance(1);
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x7E8, {0x06, 0x41, 0x00, 0x00, 0x18, 0x00, 0x00, 0x00})));
    loop();
    StreamingSimulation::advance(499);
    loop();
    TEST_ASSERT_EQUAL_HEX8(0x0C, ESP32Can.transmitted.back().data[2]);
    StreamingSimulation::advance(1);
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x7E8, {0x04, 0x41, 0x0C, 0x0A, 0x0C, 0x00, 0x00, 0x00})));
    loop();
    TEST_ASSERT_EQUAL_UINT16(182, engineRPM);  // 643 rpm OBD reference is kept separate.
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
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RPM:      182"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "Speed:    0 km/h"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "========================="));
    assertRawLinesComplete(Serial.captured);
}

void test_short_usb_write_is_not_retried_and_can_statistics_do_not_report_it() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    Serial.shortPrefix = "RX 0x182 ";
    Serial.shortLimit = 8;
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {0xDE, 0xAD, 0xBE, 0xEF})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x183, {0x12, 0x34})));
    loop();
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x182"));  // Missing payload/flags: observed limitation.
    TEST_ASSERT_FALSE(hasLine(Serial.captured, "RX 0x182 [4]: DE AD BE EF | t_ms=2000 ext=0 rtr=0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x183 [2]: 12 34 | t_ms=2000 ext=0 rtr=0"));
    unsigned attempts = 0;
    for (const auto& write : Serial.writes) {
        if (write.attempted.rfind("RX 0x182 ", 0) == 0) {
            ++attempts;
            TEST_ASSERT_EQUAL_UINT32(8, write.accepted);
            TEST_ASSERT_EQUAL_STRING("RX 0x182 [4]: DE AD BE EF | t_ms=2000 ext=0 rtr=0", write.attempted.c_str());
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1, attempts);
    TEST_ASSERT_EQUAL_UINT32(2, ESP32Can.reads.size());
    TEST_ASSERT_EQUAL_UINT32(2, canProcessor.getUnknownFrames());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.rxMissedCounter());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.txFailedCounter());
}

void test_rejected_usb_newline_concatenates_rx_and_odb_lines() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    // Arduino Print::println emits its text and CRLF as separate writes.
    // Accept the full text, reject the first CRLF, then recover immediately.
    Serial.shortPrefix = "\r\n";
    Serial.shortLimit = 0;
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {0xDE, 0xAD})));
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x7E8, {0x03, 0x41, 0x0D, 0x64, 0, 0, 0, 0})));
    loop();
    const std::string joined = "RX 0x182 [2]: DE AD | t_ms=2000 ext=0 rtr=0"
                               "ODB RAW_RX 0x7E8 [8]: 03 41 0D 64 00 00 00 00 | t_ms=2000 ext=0 rtr=0";
    TEST_ASSERT_TRUE(hasLine(Serial.captured, joined));
    TEST_ASSERT_EQUAL_UINT32(1, lines(Serial.captured).size());
    TEST_ASSERT_EQUAL_UINT32(0, countPrefix(Serial.captured, "ODB RAW_RX "));
    TEST_ASSERT_EQUAL_UINT32(2, ESP32Can.reads.size());
    unsigned rejectedNewlines = 0;
    for (const auto& write : Serial.writes) {
        if (write.attempted == "\r\n" && write.accepted == 0) ++rejectedNewlines;
    }
    TEST_ASSERT_EQUAL_UINT32(1, rejectedNewlines);
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.rxMissedCounter());
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.txFailedCounter());
}

void test_simulated_usb_block_stalls_acquisition_and_can_queue_overflows() {
    enableLoggingThroughLoop();
    Serial.clearOutput();
    const uint32_t startedAt = StreamingSimulation::now;
    const size_t previousReads = ESP32Can.reads.size();
    TEST_ASSERT_TRUE(ESP32Can.enqueue(frame(0x182, {0x00})));
    Serial.blockingPrefix = "RX ";
    Serial.blockingMs = 500;
    StreamingSimulation::onAdvance = [](uint32_t, uint32_t elapsed) {
        // Synthetic 1000 frames/s input during the single simulated USB wait.
        // There is no hardware timing claim in these numbers.
        for (uint32_t i = 0; i < elapsed; ++i) {
            ESP32Can.enqueue(frame(0x182, {static_cast<uint8_t>(i)}));
        }
    };
    loop();
    StreamingSimulation::onAdvance = {};
    TEST_ASSERT_EQUAL_UINT32(500, StreamingSimulation::now - startedAt);
    TEST_ASSERT_EQUAL_UINT32(32, ESP32Can.reads.size() - previousReads);
    TEST_ASSERT_EQUAL_UINT32(372, ESP32Can.rxMissedCounter());
    TEST_ASSERT_EQUAL_UINT32(97, ESP32Can.inRxQueue());
    TEST_ASSERT_EQUAL_UINT32(startedAt, ESP32Can.reads[previousReads].at);
    TEST_ASSERT_EQUAL_UINT32(startedAt + 500, ESP32Can.reads[previousReads + 1].at);
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x182 [1]: 00 | t_ms=2000 ext=0 rtr=0"));
    TEST_ASSERT_TRUE(hasLine(Serial.captured, "RX 0x182 [1]: 00 | t_ms=2500 ext=0 rtr=0"));
    assertRawLinesComplete(Serial.captured);
    // Recovering the USB writer allows the remaining queued frames to be drained.
    for (unsigned i = 0; i < 4; ++i) loop();
    TEST_ASSERT_EQUAL_UINT32(0, ESP32Can.inRxQueue());
    TEST_ASSERT_EQUAL_UINT32(129, ESP32Can.reads.size() - previousReads);
    TEST_ASSERT_EQUAL_UINT32(372, ESP32Can.rxMissedCounter());
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_setup_uses_real_preset_and_can_queue);
    RUN_TEST(test_no_obd_response_does_not_gate_continuous_rx);
    RUN_TEST(test_loop_drains_only_32_frames_then_continues_while_obd_waits);
    RUN_TEST(test_raw_can_keeps_payload_flags_and_unconfigured_frames);
    RUN_TEST(test_obd_reference_sys_data_and_partial_command_keep_complete_lines);
    RUN_TEST(test_short_usb_write_is_not_retried_and_can_statistics_do_not_report_it);
    RUN_TEST(test_rejected_usb_newline_concatenates_rx_and_odb_lines);
    RUN_TEST(test_simulated_usb_block_stalls_acquisition_and_can_queue_overflows);
    return UNITY_END();
}
