/**
 * Replay authentic historical CAN captures through the production decoder.
 * The capture archived in January 2026 has no confirmed capture date,
 * synchronized dashboard readings or timestamps;
 * these tests establish decoding results, not physical signal calibration.
 *
 * Optional full capture replay:
 * CAN_LOG_PATH=/absolute/path/to/capture.txt pio test -e native -f test_can_log_replay
 * Supported format: RX ID: 0x180 | DLC: 8 | Data: 04 FB 36 23 4C 00 36 00
 */

#include <unity.h>
#include <cstdlib>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>
#include <string>
#include "CanConfigProcessor.h"
#include "ConfigManager_mock.h"
#include "GlobalData.h"
#include "LittleFS.h"

static const char* EXCERPT = "test/fixtures/historical_can_excerpt.txt";
static CanConfigProcessor proc;

enum class ParseResult { SKIP, FRAME, INVALID };

// Validate DLC and the complete payload before the decoder can read any bytes.
// This test harness accepts only standard IDs used by this Nissan profile.
static ParseResult parseHistoricalLine(const std::string& line, CanFrame& frame) {
    const size_t first = line.find_first_not_of(" \t\r");
    if (first == std::string::npos || line[first] == '#') return ParseResult::SKIP;

    static const std::regex record(
        R"(^\s*RX ID: 0x([0-9A-Fa-f]{1,8}) \| DLC: ([0-9]{1,2}) \| Data:\s*(.*?)\s*$)");
    static const std::regex byteToken(R"(^[0-9A-Fa-f]{2}$)");
    std::smatch match;
    if (!std::regex_match(line, match, record)) return ParseResult::INVALID;

    const unsigned long id = std::stoul(match[1].str(), nullptr, 16);
    const unsigned long dlc = std::stoul(match[2].str(), nullptr, 10);
    if (id > 0x7FF || dlc > 8) return ParseResult::INVALID;

    CanFrame parsed = {};
    parsed.identifier = static_cast<uint32_t>(id);
    parsed.data_length_code = static_cast<uint8_t>(dlc);
    std::istringstream payload(match[3].str());
    std::string token;
    size_t count = 0;
    while (payload >> token) {
        if (count >= dlc || !std::regex_match(token, byteToken)) {
            return ParseResult::INVALID;
        }
        parsed.data[count++] = static_cast<uint8_t>(std::stoul(token, nullptr, 16));
    }
    if (count != dlc) return ParseResult::INVALID;
    frame = parsed;
    return ParseResult::FRAME;
}

struct ReplayStats {
    uint32_t parsed = 0;
    uint32_t rejected = 0;
    uint32_t handled = 0;
    uint32_t unknown = 0;
    uint32_t decoderRejected = 0;
    std::map<uint32_t, uint32_t> ids;
};

static void processParsedFrame(ReplayStats& stats, const CanFrame& frame) {
    ++stats.parsed;
    ++stats.ids[frame.identifier];
    const uint32_t unknownBefore = proc.getUnknownFrames();
    if (proc.processFrame(frame)) ++stats.handled;
    else if (proc.getUnknownFrames() != unknownBefore) ++stats.unknown;
    else ++stats.decoderRejected;
}

static ReplayStats replayFile(const char* path) {
    std::ifstream input(path);
    TEST_ASSERT_TRUE_MESSAGE(input.is_open(), "Cannot open CAN capture");
    ReplayStats stats;
    std::string line;
    while (std::getline(input, line)) {
        CanFrame frame = {};
        const ParseResult result = parseHistoricalLine(line, frame);
        if (result == ParseResult::SKIP) continue;
        if (result == ParseResult::INVALID) {
            ++stats.rejected;
            continue;
        }
        processParsedFrame(stats, frame);
    }
    TEST_ASSERT_FALSE_MESSAGE(input.bad(), "Cannot read CAN capture");
    return stats;
}

void setUp() {
    mockReset();
    proc = CanConfigProcessor();
    engineRPM = 0;
    vehicleSpeed = 0;
    fuelLevel = 0;
    currentOdo = 0;
    dteValue = 0;
    fuelConsumptionInst = 0;
    fuelConsumptionAvg = 0;
    LittleFS.basePath = "data";
    TEST_ASSERT_TRUE(proc.loadFromJson("/NissanJukeF15.json"));
}

void tearDown() {}

void test_authentic_excerpt_decodes_active_original_preset() {
    const ReplayStats stats = replayFile(EXCERPT);
    TEST_ASSERT_EQUAL_UINT32(14, stats.parsed);
    TEST_ASSERT_EQUAL_UINT32(1, stats.rejected);  // Original invalid ID 0x3FC91000.
    TEST_ASSERT_EQUAL_UINT32(12, stats.handled);
    TEST_ASSERT_EQUAL_UINT32(2, stats.unknown);  // Unconfigured 0x182 and 0x2DE.
    TEST_ASSERT_EQUAL_UINT32(0, stats.decoderRejected);
    TEST_ASSERT_EQUAL_UINT32(stats.handled, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(stats.unknown, proc.getUnknownFrames());

    // Authentic frames: bytes unchanged from the original file.
    TEST_ASSERT_EQUAL_UINT16(182, engineRPM);  // 0x04FB / 7, implausible idle RPM.
    TEST_ASSERT_EQUAL_UINT8(33, fuelLevel);   // 0x40 -> active MAP_RANGE[255,0,0,45].
    TEST_ASSERT_EQUAL_UINT32(239944, currentOdo);  // 0x03A948, independent of fuel.
    TEST_ASSERT_EQUAL_INT16(0, dteValue);  // Active 0x54C absent, not a DTE reading.
    TEST_ASSERT_EQUAL_UINT16(6, fuelConsumptionInst);  // Active byte3 counter-like.
    TEST_ASSERT_EQUAL_UINT16(111, fuelConsumptionAvg);
    TEST_ASSERT_EQUAL_UINT8(0, vehicleSpeed);  // 0x284 is absent, not a speed reading.
    TEST_ASSERT_EQUAL_UINT32(0, stats.ids.count(0x284));
    TEST_ASSERT_EQUAL_UINT32(0, stats.ids.count(0x54C));
}

static void checkConsumptionReplay(bool deferred) {
    if (deferred) {
        LittleFS.basePath = "test/fixtures";
        TEST_ASSERT_TRUE(proc.loadFromJson("/proposed_anyzlo_mappings.json"));
    }
    std::ifstream input(EXCERPT);
    TEST_ASSERT_TRUE(input.is_open());
    const uint16_t expected[] = {20, 30, 30, 30, 46, 60, 60, 60, 60, 60};
    const uint8_t counters[] = {2, 4, 6, 0, 2, 4, 6, 0, 2, 6};
    size_t index = 0;
    std::string line;
    while (std::getline(input, line)) {
        CanFrame frame = {};
        if (parseHistoricalLine(line, frame) != ParseResult::FRAME ||
            frame.identifier != 0x580) continue;
        TEST_ASSERT_LESS_THAN_UINT32(10, index);
        TEST_ASSERT_EQUAL_UINT8(counters[index], frame.data[3]);
        TEST_ASSERT_TRUE(proc.processFrame(frame));
        TEST_ASSERT_EQUAL_UINT16(deferred ? expected[index] : counters[index],
                                 fuelConsumptionInst);
        TEST_ASSERT_EQUAL_UINT16(111, fuelConsumptionAvg);
        ++index;
    }
    TEST_ASSERT_EQUAL_UINT32(10, index);
}

void test_active_consumption_replay_uses_byte3() {
    // Preserve the original preset, even though byte3 looks like a counter.
    checkConsumptionReplay(false);
}

void test_deferred_consumption_replay_uses_byte1() {
    // Candidate proposal is tested separately; it is not a flashed profile.
    checkConsumptionReplay(true);
}

void test_authentic_excerpt_decodes_deferred_proposals_separately() {
    LittleFS.basePath = "test/fixtures";
    TEST_ASSERT_TRUE(proc.loadFromJson("/proposed_anyzlo_mappings.json"));
    const ReplayStats stats = replayFile(EXCERPT);
    TEST_ASSERT_EQUAL_UINT32(13, stats.handled);
    TEST_ASSERT_EQUAL_UINT32(1, stats.unknown);
    TEST_ASSERT_EQUAL_UINT32(0, stats.decoderRejected);
    TEST_ASSERT_EQUAL_UINT16(159, engineRPM);
    TEST_ASSERT_EQUAL_UINT8(23, fuelLevel);
    TEST_ASSERT_EQUAL_UINT32(239944, currentOdo);
    TEST_ASSERT_EQUAL_INT16(375, dteValue);
    TEST_ASSERT_EQUAL_UINT16(60, fuelConsumptionInst);
    TEST_ASSERT_EQUAL_UINT16(111, fuelConsumptionAvg);
}

void test_replay_parser_rejects_incomplete_or_invalid_records() {
    const char* invalid[] = {
        "RX ID: 0x180 | DLC: 8 | Data: 04 FB",
        "RX ID: 0x180 | DLC: 1 | Data: 04 FB",
        "RX ID: 0x180 | DLC: 9 | Data: 00 00 00 00 00 00 00 00 00",
        "RX ID: 0x000 | DLC: 20 | Data: 00 00 00 00 10 C9 3F 00 10 C9 3F 75 E0 DD 78 00 00 00 00 00",
        "RX ID: 0x180 | DLC: 2 | Data: 04 GG",
        "RX ID: 0x180 | DLC: 2 | Data: 04 100",
        "RX ID: 0x3FC91000 | DLC: 0 | Data: ",
        "truncated export record"
    };
    for (const char* line : invalid) {
        CanFrame frame = {};
        TEST_ASSERT_TRUE_MESSAGE(parseHistoricalLine(line, frame) == ParseResult::INVALID,
                                 line);
    }
    TEST_ASSERT_EQUAL_UINT32(0, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(0, proc.getUnknownFrames());
}

void test_replay_parser_accepts_comments_and_complete_records() {
    CanFrame frame = {};
    TEST_ASSERT_TRUE(parseHistoricalLine("  # provenance", frame) == ParseResult::SKIP);
    TEST_ASSERT_TRUE(parseHistoricalLine(" \t\r", frame) == ParseResult::SKIP);
    TEST_ASSERT_TRUE(parseHistoricalLine(
        "RX ID: 0x2DE | DLC: 8 | Data: 00 00 80 06 F0 00 0E AC \r",
        frame) == ParseResult::FRAME);
    TEST_ASSERT_EQUAL_UINT32(0x2DE, frame.identifier);
    TEST_ASSERT_EQUAL_UINT8(8, frame.data_length_code);
    TEST_ASSERT_EQUAL_UINT8(0xAC, frame.data[7]);
}

void test_short_known_frame_is_distinct_from_unknown_frame() {
    ReplayStats stats;
    CanFrame frame = {};
    vehicleSpeed = 88;
    // A self-consistent export can still omit bytes required by the preset.
    TEST_ASSERT_TRUE(parseHistoricalLine(
        "RX ID: 0x284 | DLC: 1 | Data: 00", frame) == ParseResult::FRAME);
    processParsedFrame(stats, frame);
    TEST_ASSERT_EQUAL_UINT32(1, stats.parsed);
    TEST_ASSERT_EQUAL_UINT32(1, stats.decoderRejected);
    TEST_ASSERT_EQUAL_UINT32(0, stats.handled);
    TEST_ASSERT_EQUAL_UINT32(0, stats.unknown);
    TEST_ASSERT_EQUAL_UINT8(88, vehicleSpeed);
    TEST_ASSERT_EQUAL_UINT32(0, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(0, proc.getUnknownFrames());

    TEST_ASSERT_TRUE(parseHistoricalLine(
        "RX ID: 0x182 | DLC: 1 | Data: 00", frame) == ParseResult::FRAME);
    processParsedFrame(stats, frame);
    TEST_ASSERT_EQUAL_UINT32(2, stats.parsed);
    TEST_ASSERT_EQUAL_UINT32(1, stats.decoderRejected);
    TEST_ASSERT_EQUAL_UINT32(1, stats.unknown);
    TEST_ASSERT_EQUAL_UINT32(0, stats.handled);
    TEST_ASSERT_EQUAL_UINT8(88, vehicleSpeed);
    TEST_ASSERT_EQUAL_UINT32(0, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(1, proc.getUnknownFrames());
}

void test_optional_external_log_replay() {
    const char* path = std::getenv("CAN_LOG_PATH");
    if (!path || !*path) {
        TEST_IGNORE_MESSAGE("CAN_LOG_PATH unset: optional external capture replay skipped.");
    }
    ReplayStats stats = replayFile(path);
    TEST_ASSERT_GREATER_THAN_UINT32(0, stats.parsed);
    TEST_ASSERT_GREATER_THAN_UINT32(0, stats.handled);
    TEST_ASSERT_EQUAL_UINT32(stats.parsed,
                            stats.handled + stats.unknown + stats.decoderRejected);
    TEST_ASSERT_EQUAL_UINT32(stats.handled, proc.getFramesProcessed());
    TEST_ASSERT_EQUAL_UINT32(stats.unknown, proc.getUnknownFrames());
    printf("CAN replay: accepted=%u parser_rejected=%u handled=%u unknown=%u "
           "decoder_rejected=%u\n",
           stats.parsed, stats.rejected, stats.handled, stats.unknown,
           stats.decoderRejected);
    printf("Capture ID counts: RPM[0x180]=%u speed[0x284]=%u fuel[0x5C5]=%u "
           "DTE[0x54C]=%u deferred_DTE[0x2DE]=%u consumption[0x580]=%u\n",
           stats.ids[0x180], stats.ids[0x284], stats.ids[0x5C5],
           stats.ids[0x54C], stats.ids[0x2DE], stats.ids[0x580]);
    printf("Last decoded values: RPM=%u speed=%u fuel=%uL odo=%u "
           "DTE=%dkm instant_raw=%u average_raw=%u\n",
           engineRPM, vehicleSpeed, fuelLevel, currentOdo, dteValue,
           fuelConsumptionInst, fuelConsumptionAvg);
    printf("Replay confirms software decoding only; calibration requires dashboard readings.\n");
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_authentic_excerpt_decodes_active_original_preset);
    RUN_TEST(test_active_consumption_replay_uses_byte3);
    RUN_TEST(test_deferred_consumption_replay_uses_byte1);
    RUN_TEST(test_authentic_excerpt_decodes_deferred_proposals_separately);
    RUN_TEST(test_replay_parser_rejects_incomplete_or_invalid_records);
    RUN_TEST(test_replay_parser_accepts_comments_and_complete_records);
    RUN_TEST(test_short_known_frame_is_distinct_from_unknown_frame);
    RUN_TEST(test_optional_external_log_replay);
    return UNITY_END();
}
