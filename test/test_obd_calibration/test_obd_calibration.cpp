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

void test_log_cycles_preserve_global_spacing_even_after_failed_tx_or_millis_wrap() {
    for (const bool queued : {false, true}) for (const uint32_t start : {0u, UINT32_MAX - 100u}) {
        Harness h;
        h.acceptSend = queued;
        ObdCalibration obd(Harness::send, Harness::emit, &h);
        obd.setEnabled(true, start);
        obd.tick(start);
        TEST_ASSERT_EQUAL_UINT(1, h.attempts.size());
        obd.setEnabled(false, start + 1u);
        obd.setEnabled(true, start + 2u);
        obd.tick(start + 2u);
        TEST_ASSERT_EQUAL_UINT(1, h.attempts.size());
        TEST_ASSERT_FALSE(obd.waiting());
        // A reply from the cancelled discovery must not seed the new session.
        obd.receive(supported(0x7E8, 0, supportBit(0, 0x0C)), start + 3u);
        TEST_ASSERT_EQUAL_UINT(0, h.count("ODB SUPPORTED "));
        obd.tick(start + 499u);
        TEST_ASSERT_EQUAL_UINT(1, h.attempts.size());
        h.acceptSend = true;
        obd.tick(start + 500u);
        TEST_ASSERT_EQUAL_UINT(2, h.attempts.size());
        assertRequest(h, 0x7DF, 0);
        TEST_ASSERT_TRUE(obd.waiting());
        // A second rapid cycle must retain the most recent attempt, too.
        obd.setEnabled(false, start + 501u);
        obd.setEnabled(true, start + 502u);
        obd.tick(start + 999u);
        TEST_ASSERT_EQUAL_UINT(2, h.attempts.size());
        obd.tick(start + 1000u);
        TEST_ASSERT_EQUAL_UINT(3, h.attempts.size());
        assertRequest(h, 0x7DF, 0);
    }
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

void test_additional_service01_conversions_and_multi_value_responses() {
    struct Case { uint8_t pid; std::vector<uint8_t> bytes; const char* name; const char* number; const char* unit; };
    const Case cases[] = {
        {0x03, {2, 8}, "fuel_system_1", "2.000", "bitmask"},
        {0x04, {153}, "engine_load", "60.000", "%"},
        {0x06, {160}, "short_fuel_trim_b1", "25.000", "%"},
        {0x07, {96}, "long_fuel_trim_b1", "-25.000", "%"},
        {0x0F, {60}, "intake_air_temperature", "20.000", "degC"},
        {0x10, {0x12, 0x34}, "mass_air_flow", "46.600", "g/s"},
        {0x11, {102}, "throttle_position", "40.000", "%"},
        {0x13, {5}, "oxygen_sensors_present", "5.000", "bitmask"},
        {0x24, {0xC0, 0, 0x60, 0}, "wideband_equivalence_ratio_1", "1.500", "ratio"},
        {0x44, {0x80, 0}, "commanded_equivalence_ratio", "1.000", "ratio"},
        {0x51, {1}, "fuel_type", "1.000", "enum"},
        {0x5C, {130}, "engine_oil_temperature", "90.000", "degC"},
    };
    for (const auto& entry : cases) {
        Harness h;
        ObdCalibration obd(Harness::send, Harness::emit, &h);
        const auto now = beginSingle(obd, h, entry.pid);
        CanFrame response = {};
        response.identifier = 0x7E8;
        response.data_length_code = 8;
        response.data[0] = 2 + entry.bytes.size();
        response.data[1] = 0x41;
        response.data[2] = entry.pid;
        std::copy(entry.bytes.begin(), entry.bytes.end(), response.data + 3);
        // An incomplete ISO-TP payload must not turn padding into a measurement.
        --response.data[0];
        obd.receive(response, now + 1);
        TEST_ASSERT_EQUAL_UINT(0, h.count("ODB RX "));
        ++response.data[0];
        obd.receive(response, now + 2);
        const std::string wanted = std::string("name=") + entry.name + " value=" + entry.number + " unit=" + entry.unit;
        bool found = false;
        for (const auto& line : h.lines) if (line.find(wanted) != std::string::npos) found = true;
        TEST_ASSERT_TRUE_MESSAGE(found, wanted.c_str());
        const bool multi = entry.pid == 0x03 || entry.pid == 0x24;
        TEST_ASSERT_EQUAL_UINT(multi ? 2 : 1, h.count("ODB RX "));
        if (entry.pid == 0x03) contains(h.last("ODB RX "), "name=fuel_system_2 value=8.000 unit=bitmask");
        if (entry.pid == 0x24) contains(h.last("ODB RX "), "name=wideband_o2_voltage_1 value=3.000 unit=V");
        obd.receive(response, now + 3);
        TEST_ASSERT_EQUAL_UINT(multi ? 2 : 1, h.count("ODB RX "));
    }
}

void test_fast_references_get_priority_without_starving_slow_parameters() {
    Harness h;
    ObdCalibration obd(Harness::send, Harness::emit, &h);
    obd.setEnabled(true, 0); obd.tick(0);
    obd.receive(supported(0x7E8, 0, supportBit(0, 0x03) | supportBit(0, 0x05) |
        supportBit(0, 0x0C) | supportBit(0, 0x0D) | supportBit(0, 0x10) | 1), 1);
    obd.tick(250); obd.tick(500);
    obd.receive(supported(0x7E8, 0x20, supportBit(0x20, 0x24) | 1), 501);
    obd.tick(750); obd.tick(1000);
    obd.receive(supported(0x7E8, 0x40, supportBit(0x40, 0x44)), 1001);
    obd.tick(1250);
    uint32_t now = 1500;
    const uint8_t expected[] = {0x10, 0x03, 0x0C, 0x10, 0x24, 0x0D, 0x10, 0x44, 0x05, 0x10, 0x03, 0x0C};
    for (const auto pid : expected) {
        const auto attempts = h.attempts.size();
        obd.tick(now); assertRequest(h, 0x7E0, pid);
        obd.tick(now + 250); obd.tick(now + 499);
        TEST_ASSERT_EQUAL_UINT(attempts + 1, h.attempts.size());
        now += 500;
    }
}

namespace {
void assertProbe(const Harness& h, uint16_t did) {
    TEST_ASSERT_FALSE(h.attempts.empty());
    const auto& f = h.attempts.back();
    const uint8_t bytes[] = {3, 0x22, uint8_t(did >> 8), uint8_t(did), 0, 0, 0, 0};
    TEST_ASSERT_EQUAL_HEX32(0x7E0, f.identifier);
    TEST_ASSERT_EQUAL_UINT8(8, f.data_length_code);
    TEST_ASSERT_FALSE(f.extd); TEST_ASSERT_FALSE(f.rtr);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(bytes, f.data, 8);
}
void assertSession(const Harness& h, uint8_t command) {
    TEST_ASSERT_FALSE(h.attempts.empty());
    const auto& f = h.attempts.back();
    const uint8_t bytes[] = {2, 0x10, command, 0, 0, 0, 0, 0};
    TEST_ASSERT_EQUAL_HEX32(0x7E0, f.identifier);
    TEST_ASSERT_EQUAL_UINT8(8, f.data_length_code);
    TEST_ASSERT_FALSE(f.extd); TEST_ASSERT_FALSE(f.rtr);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(bytes, f.data, 8);
}
CanFrame sessionValue(uint8_t command) {
    CanFrame f = {};
    f.identifier = 0x7E8; f.data_length_code = 8;
    f.data[0] = 2; f.data[1] = 0x50; f.data[2] = command;
    return f;
}
CanFrame probeValue(uint16_t did, uint8_t raw) {
    CanFrame f = {};
    f.identifier = 0x7E8; f.data_length_code = 8;
    f.data[0] = 4; f.data[1] = 0x62;
    f.data[2] = did >> 8; f.data[3] = did; f.data[4] = raw;
    return f;
}
CanFrame manufacturerNegative(uint8_t service, uint8_t nrc) {
    auto f = negative(0x7E8, nrc); f.data[2] = service; return f;
}
uint32_t beginSession(ObdCalibration& obd, Harness& h, uint32_t start = 0) {
    obd.setNissanProbesEnabled(true, start);
    obd.setEnabled(true, start); obd.tick(start);
    obd.receive(supported(0x7E8, 0, supportBit(0, 0x0C)), uint32_t(start + 1));
    obd.tick(uint32_t(start + 250)); obd.tick(uint32_t(start + 500));
    assertSession(h, 0xC0);
    return start + 500;
}
uint32_t beginOil(ObdCalibration& obd, Harness& h, uint32_t start = 0) {
    auto now = beginSession(obd, h, start);
    obd.receive(sessionValue(0xC0), uint32_t(now + 1));
    obd.tick(uint32_t(now + 500)); assertProbe(h, 0x1101);
    obd.receive(probeValue(0x1101, 110), uint32_t(now + 501));
    obd.tick(uint32_t(now + 1000)); assertProbe(h, 0x111F);
    return now + 1000;
}
size_t countService(const Harness& h, uint8_t service) {
    size_t n = 0;
    for (const auto& f : h.attempts) if (f.data[1] == service) ++n;
    return n;
}
}

void test_nissan_session_requires_positive_exact_echo_before_any_did() {
    Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
    const auto now = beginSession(obd, h);
    obd.receive(probeValue(0x111F, 120), now + 1);
    for (const uint8_t echo : {0x40, 0x81}) obd.receive(sessionValue(echo), now + 2);
    auto bad = sessionValue(0xC0); bad.identifier = 0x7E9; obd.receive(bad, now + 3);
    bad = sessionValue(0xC0); bad.extd = true; obd.receive(bad, now + 3);
    bad = sessionValue(0xC0); bad.rtr = true; obd.receive(bad, now + 3);
    for (const uint8_t length : {0, 1, 8, 0x12}) {
        bad = sessionValue(0xC0); bad.data[0] = length; obd.receive(bad, now + 4);
    }
    TEST_ASSERT_EQUAL_UINT(0, h.count("ODB SESSION_RX "));
    TEST_ASSERT_EQUAL_UINT(0, h.count("ODB PROBE_RX "));
    // Optional timing bytes are retained in RAW, not mistaken for measurements.
    auto accepted = sessionValue(0xC0); accepted.data[0] = 6;
    accepted.data[3] = 0x12; accepted.data[4] = 0x34;
    obd.receive(accepted, now + 100);
    obd.receive(accepted, now + 101);
    TEST_ASSERT_EQUAL_UINT(1, h.count("ODB SESSION_RX "));
    contains(h.last("ODB SESSION_RX "), "subfunction=0xC0 status=opened length=6 latency_ms=100");
    obd.tick(now + 499); TEST_ASSERT_EQUAL_UINT(0, countService(h, 0x22));
    obd.tick(now + 500); assertProbe(h, 0x1101);
    obd.receive(probeValue(0x1101, 110), now + 501);
    contains(h.last("ODB PROBE_RX "), "name=nissan_coolant_temperature_candidate raw=0x6E value=60.000 unit=degC");
    obd.tick(now + 1000); assertProbe(h, 0x111F);
    obd.receive(probeValue(0x111F, 114), now + 1001);
    contains(h.last("ODB PROBE_RX "), "name=nissan_oil_temperature_candidate raw=0x72 value=64.000 unit=degC");
    obd.tick(now + 1500); assertSession(h, 0x81);
    obd.receive(sessionValue(0x81), now + 1501);
    contains(h.last("ODB SESSION_RX "), "subfunction=0x81 status=closed");
    obd.tick(now + 2000); assertRequest(h, 0x7E0, 0x0C);
    obd.receive(value(0x7E8, 0x0C, {0x0C, 0x80}), now + 2001);
    contains(h.last("ODB RX "), "name=rpm value=800.000 unit=rpm");
}

void test_nissan_groups_cover_all_candidates_once_and_leave_raw_units_unchanged() {
    Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
    auto now = beginSession(obd, h);
    size_t consumed = h.attempts.size() - 1;
    std::vector<uint16_t> dids;
    std::vector<uint32_t> starts;
    unsigned readsInGroup = 0;
    for (unsigned i = 0; i < 130; ++i, now += 250) {
        obd.tick(now);
        if (consumed == h.attempts.size()) continue;
        TEST_ASSERT_EQUAL_UINT(consumed + 1, h.attempts.size());
        const auto f = h.attempts[consumed++];
        if (f.data[1] == 0x10) {
            if (f.data[2] == 0xC0) { starts.push_back(now); readsInGroup = 0; }
            else { TEST_ASSERT_EQUAL_HEX8(0x81, f.data[2]); TEST_ASSERT_LESS_OR_EQUAL_UINT(2, readsInGroup); }
            obd.receive(sessionValue(f.data[2]), now + 1);
        } else if (f.data[1] == 0x22) {
            ++readsInGroup;
            const uint16_t did = uint16_t(f.data[2]) * 256 + f.data[3];
            dids.push_back(did); obd.receive(probeValue(did, 67), now + 1);
            if (did != 0x1101 && did != 0x111F) contains(h.last("ODB PROBE_RX "), "raw=0x43 value=67.000 unit=raw");
        } else TEST_ASSERT_EQUAL_HEX8(1, f.data[1]);
    }
    const uint16_t expected[] = {0x1101, 0x111F, 0x1114, 0x1183, 0x1304};
    TEST_ASSERT_EQUAL_UINT(5, dids.size());
    TEST_ASSERT_EQUAL_HEX16_ARRAY(expected, dids.data(), 5);
    TEST_ASSERT_EQUAL_UINT(3, starts.size());
    for (size_t i = 1; i < starts.size(); ++i) TEST_ASSERT_GREATER_OR_EQUAL_UINT32(10000, starts[i] - starts[i-1]);
    TEST_ASSERT_EQUAL_UINT(6, countService(h, 0x10));
    const auto manufacturer = countService(h, 0x22) + countService(h, 0x10);
    for (; now < 65000; now += 500) obd.tick(now);
    TEST_ASSERT_EQUAL_UINT(manufacturer, countService(h, 0x22) + countService(h, 0x10));
}

void test_nissan_open_refusal_or_timeout_never_reads_dids_or_retries_session() {
    for (const uint8_t nrc : {0, 0x12, 0x78, 0x80}) {
        Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
        const auto now = beginSession(obd, h);
        if (nrc) obd.receive(manufacturerNegative(0x10, nrc), now + 1);
        obd.tick(now + 250);
        TEST_ASSERT_FALSE(obd.waiting());
        TEST_ASSERT_EQUAL_UINT(nrc == 0 || nrc == 0x78 ? 1 : 0, h.count("ODB TIMEOUT "));
        obd.tick(now + 500); assertSession(h, 0x81);
        obd.receive(sessionValue(0x81), now + 501);
        for (uint32_t t = now + 750; t < now + 35000; t += 250) obd.tick(t);
        TEST_ASSERT_EQUAL_UINT(0, countService(h, 0x22));
        TEST_ASSERT_EQUAL_UINT(2, countService(h, 0x10));
        TEST_ASSERT_GREATER_THAN_UINT(2, countService(h, 1));
    }
}

void test_nissan_terminal_session_nrc_cannot_be_overridden_by_late_positive() {
    for (const bool positiveFirst : {false, true}) {
        Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
        const auto now = beginSession(obd, h);
        if (positiveFirst) obd.receive(sessionValue(0xC0), now + 1);
        obd.receive(manufacturerNegative(0x10, 0x12), now + 2);
        obd.receive(manufacturerNegative(0x10, 0x78), now + 3);
        obd.receive(sessionValue(0xC0), now + 4);
        TEST_ASSERT_EQUAL_UINT(positiveFirst ? 1 : 0, h.count("ODB SESSION_RX "));
        obd.tick(now + 500); assertSession(h, 0x81);
        TEST_ASSERT_EQUAL_UINT(0, countService(h, 0x22));
    }
}

void test_nissan_control_refusal_does_not_claim_oil_is_absent() {
    Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
    const auto now = beginSession(obd, h);
    obd.receive(sessionValue(0xC0), now + 1);
    obd.tick(now + 500); assertProbe(h, 0x1101);
    obd.receive(manufacturerNegative(0x22, 0x12), now + 501);
    obd.tick(now + 1000); assertProbe(h, 0x111F);
    obd.receive(probeValue(0x111F, 110), now + 1001);
    contains(h.last("ODB PROBE_RX "), "did=0x111F");
    obd.tick(now + 1500); assertSession(h, 0x81);
}

void test_nissan_probe_lengths_ecu_and_terminal_nrc_are_checked() {
    for (const bool positiveFirst : {false, true}) {
        Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
        const auto now = beginOil(obd, h);
        const auto prior = h.count("ODB PROBE_RX ");
        auto bad = probeValue(0x111F, 110); bad.identifier = 0x7E9; obd.receive(bad, now + 1);
        bad = probeValue(0x111F, 110); bad.extd = true; obd.receive(bad, now + 1);
        bad = probeValue(0x111F, 110); bad.rtr = true; obd.receive(bad, now + 1);
        bad = probeValue(0x111F, 110); bad.data_length_code = 7; obd.receive(bad, now + 1);
        for (const uint8_t length : {0, 3, 5, 0x14}) {
            bad = probeValue(0x111F, 110); bad.data[0] = length; obd.receive(bad, now + 1);
        }
        obd.receive(probeValue(0x1114, 110), now + 1);
        TEST_ASSERT_EQUAL_UINT(prior, h.count("ODB PROBE_RX "));
        if (positiveFirst) obd.receive(probeValue(0x111F, 110), now + 2);
        obd.receive(manufacturerNegative(0x22, 0x12), now + 3);
        obd.receive(manufacturerNegative(0x22, 0x78), now + 4);
        obd.receive(probeValue(0x111F, 110), now + 5);
        TEST_ASSERT_EQUAL_UINT(prior + (positiveFirst ? 1 : 0), h.count("ODB PROBE_RX "));
        obd.tick(now + 500); assertSession(h, 0x81);
    }
}

void test_nissan_pending_window_is_bounded_for_session_and_probe() {
    for (const bool session : {false, true}) for (const bool responds : {false, true}) {
        Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
        const auto now = session ? beginSession(obd, h) : beginOil(obd, h);
        const auto count = h.count(session ? "ODB SESSION_RX " : "ODB PROBE_RX ");
        obd.receive(manufacturerNegative(session ? 0x10 : 0x22, 0x78), now + 1);
        const auto response = session ? sessionValue(0xC0) : probeValue(0x111F, 110);
        if (responds) obd.receive(response, now + 249);
        obd.tick(now + 250);
        TEST_ASSERT_FALSE(obd.waiting());
        obd.receive(response, now + 251);
        TEST_ASSERT_EQUAL_UINT(count + (responds ? 1 : 0), h.count(session ? "ODB SESSION_RX " : "ODB PROBE_RX "));
        TEST_ASSERT_EQUAL_UINT(responds ? 0 : 1, h.count("ODB TIMEOUT "));
    }
}

void test_nissan_close_failure_blocks_further_groups_but_standard_polling_survives() {
    for (const unsigned failure : {0U, 1U, 2U}) {
        const bool sendFailure = failure == 1;
        Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
        const auto now = beginOil(obd, h);
        obd.receive(probeValue(0x111F, 110), now + 1);
        if (sendFailure) h.acceptSend = false;
        obd.tick(now + 500); assertSession(h, 0x81);
        h.acceptSend = true;
        if (failure == 2) obd.receive(manufacturerNegative(0x10, 0x12), now + 501);
        for (uint32_t t = now + 750; t < now + 35000; t += 250) obd.tick(t);
        contains(h.last("ODB SESSION_STATE "), "state=unknown");
        TEST_ASSERT_EQUAL_UINT(2, countService(h, 0x10));
        TEST_ASSERT_EQUAL_UINT(2, countService(h, 0x22));
        TEST_ASSERT_GREATER_THAN_UINT(2, countService(h, 1));
    }
}

void test_nissan_cancel_is_best_effort_and_respects_spacing() {
    for (const bool profile : {false, true}) for (const bool spaced : {false, true}) {
        Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
        const auto now = beginOil(obd, h);
        const auto attempts = h.attempts.size();
        const auto cancelledAt = now + (spaced ? 500 : 1);
        if (profile) obd.setNissanProbesEnabled(false, cancelledAt);
        else obd.setEnabled(false, cancelledAt);
        TEST_ASSERT_FALSE(obd.waiting());
        TEST_ASSERT_EQUAL_UINT(attempts + (spaced ? 1 : 0), h.attempts.size());
        contains(h.last("ODB SESSION_STATE "), "status=cancelled");
        contains(h.last("ODB SESSION_STATE "), spaced ? "close=queued_unconfirmed" : "close=request_interval");
        if (spaced) assertSession(h, 0x81);
        obd.receive(probeValue(0x111F, 110), cancelledAt + 1);
        const auto manufacturer = countService(h, 0x10) + countService(h, 0x22);
        if (profile) obd.setNissanProbesEnabled(true, cancelledAt + 2);
        for (uint32_t t = cancelledAt + 500; t < cancelledAt + 35000; t += 500) obd.tick(t);
        TEST_ASSERT_EQUAL_UINT(manufacturer, countService(h, 0x10) + countService(h, 0x22));
        if (profile) obd.setEnabled(false, cancelledAt + 35000);
        beginSession(obd, h, cancelledAt + 35001); // Only fresh LOG ON resets attempts.
    }
}

void test_nissan_cancel_close_and_immediate_log_restart_share_one_request_budget() {
    for (const bool closeQueued : {false, true}) {
        Harness h;
        ObdCalibration obd(Harness::send, Harness::emit, &h);
        const auto now = beginOil(obd, h);
        const uint32_t closedAt = now + 500;
        const auto beforeClose = h.attempts.size();
        h.acceptSend = closeQueued;
        obd.setEnabled(false, closedAt);
        assertSession(h, 0x81);
        TEST_ASSERT_EQUAL_UINT(beforeClose + 1, h.attempts.size());
        const auto sessionResponses = h.count("ODB SESSION_RX ");
        h.acceptSend = true;
        obd.setEnabled(true, closedAt + 1);
        obd.tick(closedAt + 1);
        TEST_ASSERT_EQUAL_UINT(beforeClose + 1, h.attempts.size());
        TEST_ASSERT_FALSE(obd.waiting());
        obd.receive(sessionValue(0x81), closedAt + 2);
        TEST_ASSERT_EQUAL_UINT(sessionResponses, h.count("ODB SESSION_RX "));
        obd.tick(closedAt + 499);
        TEST_ASSERT_EQUAL_UINT(beforeClose + 1, h.attempts.size());
        obd.tick(closedAt + 500);
        TEST_ASSERT_EQUAL_UINT(beforeClose + 2, h.attempts.size());
        assertRequest(h, 0x7DF, 0);
        obd.receive(supported(0x7E8, 0, supportBit(0, 0x0C)), closedAt + 501);
        obd.tick(closedAt + 1000);
        assertSession(h, 0xC0);
    }
}

void test_nissan_send_failures_do_not_fabricate_an_open_session_or_leave_it_unclosed() {
    Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
    obd.setNissanProbesEnabled(true);
    obd.setEnabled(true, 0); obd.tick(0);
    obd.receive(supported(0x7E8, 0, supportBit(0, 0x0C)), 1);
    h.acceptSend = false; obd.tick(500); assertSession(h, 0xC0);
    TEST_ASSERT_FALSE(obd.waiting());
    obd.receive(sessionValue(0xC0), 501);
    TEST_ASSERT_EQUAL_UINT(0, h.count("ODB SESSION_RX "));
    h.acceptSend = true; obd.tick(1000); assertRequest(h, 0x7E0, 0x0C);
    TEST_ASSERT_EQUAL_UINT(0, countService(h, 0x22));
    obd.setEnabled(false, 1001);
    const auto now = beginSession(obd, h, 2000);
    obd.receive(sessionValue(0xC0), now + 1);
    h.acceptSend = false; obd.tick(now + 500); assertProbe(h, 0x1101);
    TEST_ASSERT_FALSE(obd.waiting());
    h.acceptSend = true; obd.tick(now + 1000); assertSession(h, 0x81);
}

void test_nissan_deadline_wrap_and_broadcast_interleaving_do_not_extend_session() {
    Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
    const auto now = beginSession(obd, h, UINT32_MAX - 700);
    // An ordinary broadcast must remain byte-for-byte available to RAW formatting.
    auto broadcast = probeValue(0x111F, 120); broadcast.identifier = 0x180;
    char raw[160]; TEST_ASSERT_TRUE(formatCanFrameLog(broadcast, uint32_t(now + 10), raw, sizeof(raw)));
    const auto original = broadcast;
    obd.receive(broadcast, uint32_t(now + 10));
    TEST_ASSERT_EQUAL_MEMORY(original.data, broadcast.data, 8);
    TEST_ASSERT_EQUAL_UINT(0, h.count("ODB PROBE_RX "));
    obd.receive(sessionValue(0xC0), uint32_t(now + 200));
    contains(h.last("ODB SESSION_RX "), "latency_ms=200");
    obd.tick(uint32_t(now + 2000)); // Scheduler was delayed; skip all data reads.
    assertSession(h, 0x81);
    contains(h.last("ODB SESSION_STATE "), "status=read_deadline");
    TEST_ASSERT_EQUAL_UINT(0, countService(h, 0x22));
    obd.receive(sessionValue(0x81), uint32_t(now + 2001));
    obd.tick(uint32_t(now + 2500)); assertRequest(h, 0x7E0, 0x0C);
}

void test_nissan_probes_are_opt_in_and_require_engine_ecu_support() {
    for (const bool enabled : {false, true}) {
        Harness h; ObdCalibration obd(Harness::send, Harness::emit, &h);
        obd.setNissanProbesEnabled(enabled);
        obd.setEnabled(true, 0); obd.tick(0);
        obd.receive(supported(enabled ? 0x7E9 : 0x7E8, 0, supportBit(0, 0x0C)), 1);
        for (uint32_t now = 250; now < 50000; now += 250) obd.tick(now);
        for (const auto& f : h.attempts) TEST_ASSERT_EQUAL_UINT8(1, f.data[1]);
    }
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
    RUN_TEST(test_log_cycles_preserve_global_spacing_even_after_failed_tx_or_millis_wrap);
    RUN_TEST(test_millis_wrap_preserves_latency_timeout_and_request_spacing);
    RUN_TEST(test_raw_formatter_preserves_payload_and_records_dequeue_time);
    RUN_TEST(test_raw_formatter_marks_only_standard_diagnostic_identifiers);
    RUN_TEST(test_raw_formatter_reports_malformed_dlc_without_reading_extra_bytes);
    RUN_TEST(test_raw_formatter_rtr_never_invents_payload);
    RUN_TEST(test_raw_formatter_zero_dlc_has_empty_payload);
    RUN_TEST(test_raw_formatter_rejects_null_or_small_buffer_before_writing);
    RUN_TEST(test_additional_service01_conversions_and_multi_value_responses);
    RUN_TEST(test_fast_references_get_priority_without_starving_slow_parameters);
    RUN_TEST(test_nissan_session_requires_positive_exact_echo_before_any_did);
    RUN_TEST(test_nissan_groups_cover_all_candidates_once_and_leave_raw_units_unchanged);
    RUN_TEST(test_nissan_open_refusal_or_timeout_never_reads_dids_or_retries_session);
    RUN_TEST(test_nissan_terminal_session_nrc_cannot_be_overridden_by_late_positive);
    RUN_TEST(test_nissan_control_refusal_does_not_claim_oil_is_absent);
    RUN_TEST(test_nissan_probe_lengths_ecu_and_terminal_nrc_are_checked);
    RUN_TEST(test_nissan_pending_window_is_bounded_for_session_and_probe);
    RUN_TEST(test_nissan_close_failure_blocks_further_groups_but_standard_polling_survives);
    RUN_TEST(test_nissan_cancel_is_best_effort_and_respects_spacing);
    RUN_TEST(test_nissan_cancel_close_and_immediate_log_restart_share_one_request_budget);
    RUN_TEST(test_nissan_send_failures_do_not_fabricate_an_open_session_or_leave_it_unclosed);
    RUN_TEST(test_nissan_deadline_wrap_and_broadcast_interleaving_do_not_extend_session);
    RUN_TEST(test_nissan_probes_are_opt_in_and_require_engine_ecu_support);
    return UNITY_END();
}
