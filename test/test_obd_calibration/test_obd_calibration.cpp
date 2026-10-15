/** Native protocol and scheduling tests; no CAN hardware or serial device. */
#include <unity.h>
#include "ObdCalibration.h"
#include "CanLogFormatter.h"

#include <initializer_list>
#include <string>
#include <vector>

namespace {
struct Harness {
    bool acceptSend = true;
    std::vector<CanFrame> attempts;
    std::vector<std::string> lines;

    static bool send(const CanFrame& frame, void* context) {
        auto& self = *static_cast<Harness*>(context);
        self.attempts.push_back(frame);
        return self.acceptSend;
    }
    static void emit(const char* line, void* context) {
        static_cast<Harness*>(context)->lines.emplace_back(line);
    }
    size_t count(const char* prefix) const {
        size_t found = 0;
        for (const auto& line : lines) if (line.rfind(prefix, 0) == 0) ++found;
        return found;
    }
    std::string last(const char* prefix) const {
        for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
            if (it->rfind(prefix, 0) == 0) return *it;
        }
        return {};
    }
};

uint32_t supportBit(uint8_t base, uint8_t pid) {
    return uint32_t(1) << (32 - (pid - base));
}

CanFrame supported(uint32_t ecu, uint8_t page, uint32_t bitmap) {
    CanFrame frame = {};
    frame.identifier = ecu;
    frame.data_length_code = 8;
    frame.data[0] = 6;
    frame.data[1] = 0x41;
    frame.data[2] = page;
    for (unsigned i = 0; i < 4; ++i) frame.data[3 + i] = bitmap >> (24 - i * 8);
    frame.data[7] = 0xAA;  // Padding has no signal meaning.
    return frame;
}

CanFrame value(uint32_t ecu, uint8_t pid, std::initializer_list<uint8_t> bytes) {
    CanFrame frame = {};
    frame.identifier = ecu;
    frame.data_length_code = 8;
    frame.data[0] = 2 + bytes.size();
    frame.data[1] = 0x41;
    frame.data[2] = pid;
    unsigned i = 3;
    for (const auto byte : bytes) frame.data[i++] = byte;
    while (i < 8) frame.data[i++] = 0xCC;
    return frame;
}

CanFrame negative(uint32_t ecu, uint8_t nrc = 0x12) {
    CanFrame frame = {};
    frame.identifier = ecu;
    frame.data_length_code = 8;
    frame.data[0] = 3;
    frame.data[1] = 0x7F;
    frame.data[2] = 1;
    frame.data[3] = nrc;
    return frame;
}

void contains(const std::string& line, const char* fragment) {
    TEST_ASSERT_TRUE_MESSAGE(line.find(fragment) != std::string::npos, line.c_str());
}

void assertRequest(const Harness& harness, uint32_t identifier, uint8_t pid) {
    TEST_ASSERT_FALSE(harness.attempts.empty());
    const auto& frame = harness.attempts.back();
    TEST_ASSERT_EQUAL_UINT32(identifier, frame.identifier);
    TEST_ASSERT_EQUAL_UINT8(8, frame.data_length_code);
    TEST_ASSERT_FALSE(frame.extd);
    TEST_ASSERT_FALSE(frame.rtr);
    const uint8_t expected[] = {2, 1, pid, 0, 0, 0, 0, 0};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, frame.data, 8);
}

// Discover precisely one parameter and leave its first physical query pending.
uint32_t beginSingle(ObdCalibration& obd, Harness& harness, uint8_t pid,
                     uint32_t start = 0, uint32_t ecu = 0x7E8) {
    obd.setEnabled(true, start);
    obd.tick(start);
    const uint8_t lastPage = (pid - 1) / 32;
    uint32_t now = start;
    for (uint8_t page = 0; page <= lastPage; ++page) {
        const uint8_t base = page * 32;
        assertRequest(harness, 0x7DF, base);
        const uint32_t bitmap = page == lastPage ? supportBit(base, pid) : 1;
        obd.receive(supported(ecu, base, bitmap), now + 1);
        obd.tick(now + ObdCalibration::RESPONSE_WINDOW_MS);
        now += ObdCalibration::REQUEST_INTERVAL_MS;
        obd.tick(now);
    }
    assertRequest(harness, ecu - 8, pid);
    TEST_ASSERT_TRUE(obd.waiting());
    return now;
}
}

void setUp() {}
void tearDown() {}

void test_disabled_never_transmits_or_decodes() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.tick(0);
    obd.tick(50000);
    obd.receive(supported(0x7E8, 0, 0xFFFFFFFF), 50001);
    TEST_ASSERT_FALSE(obd.enabled());
    TEST_ASSERT_FALSE(obd.waiting());
    TEST_ASSERT_TRUE(harness.attempts.empty());
    TEST_ASSERT_TRUE(harness.lines.empty());
}

void test_discovery_wire_format_and_odb_log_prefix() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 100);
    obd.tick(100);
    assertRequest(harness, 0x7DF, 0);
    contains(harness.last("ODB TX "), "t_ms=100 seq=1");
    contains(harness.last("ODB TX "), "status=queued data=02 01 00 00 00 00 00 00");
    for (const auto& line : harness.lines) TEST_ASSERT_TRUE(line.rfind("ODB ", 0) == 0);
}

void test_supported_chain_collects_multiple_ecus_and_targets_physical_owner() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 0);
    obd.tick(0);
    // First ECU lacks the continuation; last ECU contributes three pages.
    obd.receive(supported(0x7E8, 0, supportBit(0, 0x0D)), 5);
    obd.receive(supported(0x7EF, 0, supportBit(0, 0x0C) | 1), 200);
    TEST_ASSERT_EQUAL_UINT(2, harness.count("ODB SUPPORTED "));
    obd.tick(249);
    TEST_ASSERT_EQUAL_UINT(1, harness.attempts.size());
    obd.tick(250);
    obd.tick(499);
    TEST_ASSERT_EQUAL_UINT(1, harness.attempts.size());
    obd.tick(500);
    assertRequest(harness, 0x7DF, 0x20);
    // Ignore a page announced by an ECU that did not advertise its support.
    obd.receive(supported(0x7E8, 0x20, 0xFFFFFFFF), 501);
    TEST_ASSERT_EQUAL_UINT(2, harness.count("ODB SUPPORTED "));
    obd.receive(supported(0x7EF, 0x20, supportBit(0x20, 0x2F) | 1), 510);
    obd.tick(750);
    obd.tick(1000);
    assertRequest(harness, 0x7DF, 0x40);
    obd.receive(supported(0x7EF, 0x40, supportBit(0x40, 0x42) | supportBit(0x40, 0x46)), 1010);
    obd.tick(1250);
    obd.tick(1500);
    assertRequest(harness, 0x7E7, 0x0C);
    obd.receive(value(0x7EF, 0x0C, {0x09, 0xC5}), 1510);
    contains(harness.last("ODB RX "), "request_id=0x7E7 ecu=0x7EF pid=0x0C");
    obd.tick(1750);
    obd.tick(2000);
    assertRequest(harness, 0x7E0, 0x0D);
}

void test_lowest_supported_ecu_is_selected_and_other_ecu_is_ignored() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 0);
    obd.tick(0);
    obd.receive(supported(0x7EA, 0, supportBit(0, 0x0C)), 1);
    obd.receive(supported(0x7E9, 0, supportBit(0, 0x0C)), 2);
    obd.tick(250);
    obd.tick(500);
    assertRequest(harness, 0x7E1, 0x0C);
    obd.receive(value(0x7EA, 0x0C, {0x09, 0xC5}), 510);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB RX "));
    obd.receive(value(0x7E9, 0x0C, {0x09, 0xC5}), 511);
    TEST_ASSERT_EQUAL_UINT(1, harness.count("ODB RX "));
}

void test_bitmap_high_bit_does_not_shift_pid_number_or_invent_reference() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 0);
    obd.tick(0);
    obd.receive(supported(0x7E8, 0, 0x80000000), 1); // PID01 only.
    obd.tick(250);
    contains(harness.last("ODB STATE "), "status=no_supported_reference_pids");
    obd.tick(5000);
    TEST_ASSERT_EQUAL_UINT(1, harness.attempts.size());
    obd.tick(5250);
    assertRequest(harness, 0x7DF, 0);
}

void test_discovery_duplicate_cannot_replace_bitmap_with_different_data() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 0);
    obd.tick(0);
    obd.receive(supported(0x7E8, 0, supportBit(0, 0x0C)), 1);
    obd.receive(supported(0x7E8, 0, 1), 2);
    TEST_ASSERT_EQUAL_UINT(1, harness.count("ODB SUPPORTED "));
    obd.tick(250);
    obd.tick(500);
    assertRequest(harness, 0x7E0, 0x0C);
}

void test_all_eight_pid_conversions_keep_units_and_fractional_values() {
    struct Case { uint8_t pid; uint8_t a; uint8_t b; bool twoBytes;
                  const char* name; const char* number; const char* unit; };
    const Case cases[] = {
        {0x0C, 0x09, 0xC5, true, "rpm", "625.250", "rpm"},
        {0x0D, 127, 0, false, "speed", "127.000", "km/h"},
        {0x05, 0, 0, false, "coolant", "-40.000", "degC"},
        {0x2F, 128, 0, false, "fuel_level", "50.196", "%"},
        {0x42, 0x35, 0xE8, true, "module_voltage", "13.800", "V"},
        {0x46, 255, 0, false, "ambient", "215.000", "degC"},
        {0x0B, 98, 0, false, "manifold_absolute_pressure", "98.000", "kPa"},
        {0x5E, 0x00, 0xFB, true, "fuel_rate", "12.550", "L/h"},
    };
    for (const auto& entry : cases) {
        Harness harness;
        ObdCalibration obd(Harness::send, Harness::emit, &harness);
        const uint32_t now = beginSingle(obd, harness, entry.pid);
        const auto frame = entry.twoBytes ? value(0x7E8, entry.pid, {entry.a, entry.b})
                                          : value(0x7E8, entry.pid, {entry.a});
        obd.receive(frame, now + 17);
        const std::string line = harness.last("ODB RX ");
        contains(line, (std::string("name=") + entry.name).c_str());
        contains(line, (std::string("value=") + entry.number).c_str());
        contains(line, (std::string("unit=") + entry.unit + " latency_ms=17").c_str());
        contains(line, (std::string("t_ms=") + std::to_string(now + 17)).c_str());
    }
}

void test_conversion_extremes_are_standard_values_not_sentinels() {
    struct Case { uint8_t pid; bool twoBytes; const char* number; };
    const Case cases[] = {
        {0x0C, true, "16383.750"}, {0x42, true, "65.535"},
        {0x2F, false, "100.000"}, {0x05, false, "215.000"},
        {0x0D, false, "255.000"}, {0x0B, false, "255.000"},
        {0x5E, true, "3276.750"},
    };
    for (const auto& entry : cases) {
        Harness harness;
        ObdCalibration obd(Harness::send, Harness::emit, &harness);
        const uint32_t now = beginSingle(obd, harness, entry.pid);
        obd.receive(entry.twoBytes ? value(0x7E8, entry.pid, {255, 255})
                                  : value(0x7E8, entry.pid, {255}), now + 1);
        contains(harness.last("ODB RX "), (std::string("value=") + entry.number).c_str());
    }
}

void test_supported_parameters_poll_in_a_complete_cycle_without_unadvertised_pids() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 0);
    obd.tick(0);
    obd.receive(supported(0x7EA, 0, supportBit(0, 0x05) | supportBit(0, 0x0B) |
                          supportBit(0, 0x0C) | supportBit(0, 0x0D) | 1), 1);
    obd.tick(250); obd.tick(500);
    obd.receive(supported(0x7EA, 0x20, supportBit(0x20, 0x2F) | 1), 501);
    obd.tick(750); obd.tick(1000);
    obd.receive(supported(0x7EA, 0x40, supportBit(0x40, 0x42) |
                          supportBit(0x40, 0x46) | supportBit(0x40, 0x5E)), 1001);
    obd.tick(1250);
    uint32_t now = 1500;
    // The last query proves that the round-robin cycle wraps after fuel rate.
    const uint8_t expected[] = {0x0C, 0x0D, 0x05, 0x2F, 0x42, 0x46, 0x0B, 0x5E, 0x0C};
    for (const auto pid : expected) {
        obd.tick(now);
        assertRequest(harness, 0x7E2, pid);
        obd.tick(now + 250);
        now += 500;
    }
    TEST_ASSERT_EQUAL_UINT(3 + sizeof(expected), harness.attempts.size());
}

void test_invalid_sf_lengths_flags_modes_and_identifiers_never_decode() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    const uint32_t now = beginSingle(obd, harness, 0x0C);
    const CanFrame good = value(0x7E8, 0x0C, {0x09, 0xC5});
    std::vector<CanFrame> invalid;
    for (const uint8_t dlc : {0, 4, 7, 9}) {
        auto frame = good; frame.data_length_code = dlc; invalid.push_back(frame);
    }
    for (const uint8_t pci : {0, 1, 3, 5, 7, 8, 0x14, 0x24, 0x34}) {
        auto frame = good; frame.data[0] = pci; invalid.push_back(frame);
    }
    for (const uint32_t identifier : {0x7DF, 0x7E7, 0x7E9, 0x7F0, 0x18DAF110}) {
        auto frame = good; frame.identifier = identifier; invalid.push_back(frame);
    }
    auto frame = good; frame.extd = true; invalid.push_back(frame);
    frame = good; frame.rtr = true; invalid.push_back(frame);
    frame = good; frame.data[1] = 0x01; invalid.push_back(frame);
    frame = good; frame.data[1] = 0x42; invalid.push_back(frame);
    frame = good; frame.data[2] = 0x0D; invalid.push_back(frame);
    for (const auto& bad : invalid) obd.receive(bad, now + 1);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB RX "));
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB NEGATIVE "));
    TEST_ASSERT_TRUE(obd.waiting());
    obd.receive(good, now + 2);
    TEST_ASSERT_EQUAL_UINT(1, harness.count("ODB RX "));
}

void test_supported_page_must_have_exact_six_byte_payload() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 0);
    obd.tick(0);
    auto frame = supported(0x7E8, 0, supportBit(0, 0x0C));
    for (const uint8_t length : {0, 3, 4, 5, 7, 8}) {
        frame.data[0] = length;
        obd.receive(frame, 1);
    }
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB SUPPORTED "));
    frame.data[0] = 6;
    obd.receive(frame, 2);
    TEST_ASSERT_EQUAL_UINT(1, harness.count("ODB SUPPORTED "));
}

void test_duplicate_values_are_logged_once_and_window_remains_open() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    const uint32_t now = beginSingle(obd, harness, 0x0C);
    obd.receive(value(0x7E8, 0x0C, {0x09, 0xC5}), now + 1);
    obd.receive(value(0x7E8, 0x0C, {0x10, 0x00}), now + 2);
    TEST_ASSERT_EQUAL_UINT(1, harness.count("ODB RX "));
    TEST_ASSERT_TRUE(obd.waiting());
    obd.tick(now + 249);
    TEST_ASSERT_TRUE(obd.waiting());
    obd.tick(now + 250);
    TEST_ASSERT_FALSE(obd.waiting());
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB TIMEOUT "));
}

void test_negative_response_is_distinct_from_measurement_and_deduplicated() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    const uint32_t now = beginSingle(obd, harness, 0x0C);
    obd.receive(negative(0x7E8, 0x12), now + 2);
    obd.receive(negative(0x7E8, 0x12), now + 3);
    TEST_ASSERT_EQUAL_UINT(1, harness.count("ODB NEGATIVE "));
    contains(harness.last("ODB NEGATIVE "), "pid=0x0C nrc=0x12");
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB RX "));
    obd.tick(now + 250);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB TIMEOUT "));
}

void test_malformed_negative_response_does_not_suppress_timeout() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    const uint32_t now = beginSingle(obd, harness, 0x0C);
    auto frame = negative(0x7E8);
    frame.data[2] = 0x02; obd.receive(frame, now + 1);
    frame = negative(0x7E8); frame.data[0] = 4; obd.receive(frame, now + 2);
    frame = negative(0x7E9); obd.receive(frame, now + 3);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB NEGATIVE "));
    obd.tick(now + 250);
    TEST_ASSERT_EQUAL_UINT(1, harness.count("ODB TIMEOUT "));
}

void test_tx_failure_does_not_arm_waiting_or_accept_a_response() {
    Harness harness;
    harness.acceptSend = false;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 0);
    obd.tick(0);
    TEST_ASSERT_FALSE(obd.waiting());
    contains(harness.last("ODB TX "), "status=failed");
    obd.receive(supported(0x7E8, 0, 0xFFFFFFFF), 1);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB SUPPORTED "));
    obd.tick(499);
    TEST_ASSERT_EQUAL_UINT(1, harness.attempts.size());
    harness.acceptSend = true;
    obd.tick(500);
    TEST_ASSERT_EQUAL_UINT(2, harness.attempts.size());
    assertRequest(harness, 0x7DF, 0);
    TEST_ASSERT_TRUE(obd.waiting());
    contains(harness.last("ODB TX "), "status=queued");
}

void test_missing_callbacks_are_safe_and_do_not_fabricate_transmission() {
    ObdCalibration obd(nullptr, nullptr);
    obd.setEnabled(true, 0);
    obd.tick(0);
    TEST_ASSERT_TRUE(obd.enabled());
    TEST_ASSERT_FALSE(obd.waiting());
    obd.receive(supported(0x7E8, 0, 0xFFFFFFFF), 1);
    obd.setEnabled(false, 2);
    TEST_ASSERT_FALSE(obd.enabled());
}

void test_three_discovery_timeouts_then_five_second_retry() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    obd.setEnabled(true, 0);
    obd.tick(0);
    obd.tick(249);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB TIMEOUT "));
    obd.tick(250);
    contains(harness.last("ODB TIMEOUT "), "pid=0x00 elapsed_ms=250");
    obd.tick(500); obd.tick(750);
    obd.tick(1000); obd.tick(1250);
    TEST_ASSERT_EQUAL_UINT(3, harness.attempts.size());
    TEST_ASSERT_EQUAL_UINT(3, harness.count("ODB TIMEOUT "));
    contains(harness.last("ODB STATE "), "status=no_supported_response retry_ms=5000");
    obd.tick(6249);
    TEST_ASSERT_EQUAL_UINT(3, harness.attempts.size());
    obd.tick(6250);
    TEST_ASSERT_EQUAL_UINT(4, harness.attempts.size());
    assertRequest(harness, 0x7DF, 0);
}

void test_poll_timeout_keeps_supported_reference_and_never_invents_zero() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    const uint32_t now = beginSingle(obd, harness, 0x0C);
    obd.tick(now + 250);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB RX "));
    contains(harness.last("ODB TIMEOUT "), "request_id=0x7E0 pid=0x0C");
    obd.tick(now + 499);
    TEST_ASSERT_EQUAL_UINT(2, harness.attempts.size());
    obd.tick(now + 500);
    assertRequest(harness, 0x7E0, 0x0C);
    TEST_ASSERT_EQUAL_UINT(3, harness.attempts.size());
}

void test_response_at_window_boundary_is_too_late() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    const uint32_t now = beginSingle(obd, harness, 0x0C);
    obd.receive(value(0x7E8, 0x0C, {0x09, 0xC5}), now + 250);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB RX "));
    obd.tick(now + 250);
    TEST_ASSERT_EQUAL_UINT(1, harness.count("ODB TIMEOUT "));
}

void test_enabled_is_idempotent_and_off_cancels_pending_state() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    const uint32_t now = beginSingle(obd, harness, 0x0C);
    const size_t linesBefore = harness.lines.size();
    obd.setEnabled(true, now + 1);
    obd.tick(now + 2);
    TEST_ASSERT_TRUE(obd.waiting());
    TEST_ASSERT_EQUAL_UINT(linesBefore, harness.lines.size());
    TEST_ASSERT_EQUAL_UINT(2, harness.attempts.size());
    obd.setEnabled(false, now + 3);
    TEST_ASSERT_FALSE(obd.enabled());
    TEST_ASSERT_FALSE(obd.waiting());
    obd.receive(value(0x7E8, 0x0C, {0x09, 0xC5}), now + 4);
    obd.tick(now + 6000);
    TEST_ASSERT_EQUAL_UINT(0, harness.count("ODB RX "));
    TEST_ASSERT_EQUAL_UINT(2, harness.attempts.size());
    const size_t stoppedLines = harness.lines.size();
    obd.setEnabled(false, now + 6001);
    TEST_ASSERT_EQUAL_UINT(stoppedLines, harness.lines.size());
    obd.setEnabled(true, now + 6002);
    obd.tick(now + 6002);
    assertRequest(harness, 0x7DF, 0);
    // Previous ECU support cannot survive a fresh calibration session.
    obd.tick(now + 6252);
    obd.tick(now + 6502);
    assertRequest(harness, 0x7DF, 0);
}

void test_millis_wrap_preserves_latency_timeout_and_request_spacing() {
    Harness harness;
    ObdCalibration obd(Harness::send, Harness::emit, &harness);
    const uint32_t start = UINT32_MAX - 600;
    const uint32_t now = beginSingle(obd, harness, 0x0C, start);
    obd.receive(value(0x7E8, 0x0C, {0x09, 0xC5}), uint32_t(now + 200));
    contains(harness.last("ODB RX "), "latency_ms=200");
    obd.tick(uint32_t(now + 250));
    TEST_ASSERT_FALSE(obd.waiting());
    obd.tick(uint32_t(now + 499));
    TEST_ASSERT_EQUAL_UINT(2, harness.attempts.size());
    obd.tick(uint32_t(now + 500));
    TEST_ASSERT_EQUAL_UINT(3, harness.attempts.size());
    assertRequest(harness, 0x7E0, 0x0C);
    obd.tick(uint32_t(now + 750));
    contains(harness.last("ODB TIMEOUT "), "elapsed_ms=250");
}

void test_raw_formatter_preserves_payload_and_records_dequeue_time() {
    const CanFrame frame = value(0x180, 0x0C, {0x09, 0xC5});
    const uint8_t bytes[] = {4, 0x41, 0x0C, 0x09, 0xC5, 0xCC, 0xCC, 0xCC};
    char line[160];
    TEST_ASSERT_TRUE(formatCanFrameLog(frame, 123456, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING(
        "RX 0x180 [8]: 04 41 0C 09 C5 CC CC CC | t_ms=123456 ext=0 rtr=0", line);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(bytes, frame.data, 8);
    TEST_ASSERT_EQUAL_UINT32(0x180, frame.identifier);
    TEST_ASSERT_EQUAL_UINT8(8, frame.data_length_code);
}

void test_raw_formatter_marks_only_standard_diagnostic_identifiers() {
    char line[160];
    auto frame = value(0x7DF, 0x0C, {0x09, 0xC5});
    for (const uint32_t identifier : {0x7DF, 0x7E0, 0x7E8, 0x7EF}) {
        frame.identifier = identifier;
        TEST_ASSERT_TRUE(formatCanFrameLog(frame, 5, line, sizeof(line)));
        TEST_ASSERT_TRUE(std::string(line).rfind("ODB RAW_RX ", 0) == 0);
    }
    for (const uint32_t identifier : {0x7DE, 0x7F0, 0x180}) {
        frame.identifier = identifier;
        TEST_ASSERT_TRUE(formatCanFrameLog(frame, 5, line, sizeof(line)));
        TEST_ASSERT_TRUE(std::string(line).rfind("RX ", 0) == 0);
    }
    frame.identifier = 0x7E8;
    frame.extd = true;
    TEST_ASSERT_TRUE(formatCanFrameLog(frame, UINT32_MAX, line, sizeof(line)));
    TEST_ASSERT_TRUE(std::string(line).rfind("RX ", 0) == 0);
    contains(line, "t_ms=4294967295 ext=1 rtr=0");
}

void test_raw_formatter_reports_malformed_dlc_without_reading_extra_bytes() {
    CanFrame frame = {};
    frame.identifier = 0x180;
    frame.data_length_code = 20;
    for (uint8_t i = 0; i < 8; ++i) frame.data[i] = i + 1;
    const uint8_t expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    char line[160];
    TEST_ASSERT_TRUE(formatCanFrameLog(frame, 19, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING(
        "RX 0x180 [20]: 01 02 03 04 05 06 07 08 | t_ms=19 ext=0 rtr=0", line);
    TEST_ASSERT_EQUAL_UINT8(20, frame.data_length_code);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, frame.data, 8);
}

void test_raw_formatter_rtr_never_invents_payload() {
    auto frame = value(0x180, 0x0C, {0x09, 0xC5});
    frame.rtr = true;
    char line[160];
    TEST_ASSERT_TRUE(formatCanFrameLog(frame, 8, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("RX 0x180 [8]: | t_ms=8 ext=0 rtr=1", line);
    TEST_ASSERT_EQUAL_UINT8(4, frame.data[0]);
    TEST_ASSERT_TRUE(frame.rtr);
}

void test_raw_formatter_zero_dlc_has_empty_payload() {
    CanFrame frame = {};
    char line[160];
    TEST_ASSERT_TRUE(formatCanFrameLog(frame, 0, line, sizeof(line)));
    TEST_ASSERT_EQUAL_STRING("RX 0x000 [0]: | t_ms=0 ext=0 rtr=0", line);
}

void test_raw_formatter_rejects_null_or_small_buffer_before_writing() {
    const auto frame = value(0x180, 0x0C, {0x09, 0xC5});
    char line[160] = "untouched";
    TEST_ASSERT_FALSE(formatCanFrameLog(frame, 0, nullptr, sizeof(line)));
    TEST_ASSERT_FALSE(formatCanFrameLog(frame, 0, line, 159));
    TEST_ASSERT_EQUAL_STRING("untouched", line);
    TEST_ASSERT_FALSE(formatCanFrameLog(frame, 0, line, 0));
    TEST_ASSERT_EQUAL_STRING("untouched", line);
    TEST_ASSERT_TRUE(formatCanFrameLog(frame, 0, line, sizeof(line)));
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_disabled_never_transmits_or_decodes);
    RUN_TEST(test_discovery_wire_format_and_odb_log_prefix);
    RUN_TEST(test_supported_chain_collects_multiple_ecus_and_targets_physical_owner);
    RUN_TEST(test_lowest_supported_ecu_is_selected_and_other_ecu_is_ignored);
    RUN_TEST(test_bitmap_high_bit_does_not_shift_pid_number_or_invent_reference);
    RUN_TEST(test_discovery_duplicate_cannot_replace_bitmap_with_different_data);
    RUN_TEST(test_all_eight_pid_conversions_keep_units_and_fractional_values);
    RUN_TEST(test_conversion_extremes_are_standard_values_not_sentinels);
    RUN_TEST(test_supported_parameters_poll_in_a_complete_cycle_without_unadvertised_pids);
    RUN_TEST(test_invalid_sf_lengths_flags_modes_and_identifiers_never_decode);
    RUN_TEST(test_supported_page_must_have_exact_six_byte_payload);
    RUN_TEST(test_duplicate_values_are_logged_once_and_window_remains_open);
    RUN_TEST(test_negative_response_is_distinct_from_measurement_and_deduplicated);
    RUN_TEST(test_malformed_negative_response_does_not_suppress_timeout);
    RUN_TEST(test_tx_failure_does_not_arm_waiting_or_accept_a_response);
    RUN_TEST(test_missing_callbacks_are_safe_and_do_not_fabricate_transmission);
    RUN_TEST(test_three_discovery_timeouts_then_five_second_retry);
    RUN_TEST(test_poll_timeout_keeps_supported_reference_and_never_invents_zero);
    RUN_TEST(test_response_at_window_boundary_is_too_late);
    RUN_TEST(test_enabled_is_idempotent_and_off_cancels_pending_state);
    RUN_TEST(test_millis_wrap_preserves_latency_timeout_and_request_spacing);
    RUN_TEST(test_raw_formatter_preserves_payload_and_records_dequeue_time);
    RUN_TEST(test_raw_formatter_marks_only_standard_diagnostic_identifiers);
    RUN_TEST(test_raw_formatter_reports_malformed_dlc_without_reading_extra_bytes);
    RUN_TEST(test_raw_formatter_rtr_never_invents_payload);
    RUN_TEST(test_raw_formatter_zero_dlc_has_empty_payload);
    RUN_TEST(test_raw_formatter_rejects_null_or_small_buffer_before_writing);
    return UNITY_END();
}
