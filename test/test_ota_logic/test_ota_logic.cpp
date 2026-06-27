/**
 * @file test_ota_logic.cpp
 * @brief Unit tests for OTA pure-logic functions (runs on host, no ESP32 required).
 *
 * Tests:
 *   - base64Decode: valid data, empty input, invalid characters, buffer overflow
 *   - crc32_le: known reference vector, cross-platform compatibility
 *
 * The implementations here are self-contained (no Arduino/ESP-IDF dependencies)
 * so they can be executed by `pio test -e native`.
 *
 * Cross-platform CRC32 compatibility:
 *   crc32_le(0, data, len) == Python binascii.crc32(data) & 0xFFFFFFFF
 *                          == Java new CRC32().update(data); getValue()
 *                          == esp_rom_crc32_le(0, data, len)  [on ESP32]
 */

#include <unity.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// =============================================================================
// STANDALONE IMPLEMENTATIONS (no Arduino/ESP-IDF dependencies)
// =============================================================================

static size_t base64Decode(const char* input, uint8_t* output, size_t maxLen) {
    static const int8_t lut[256] = {
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,
        52,53,54,55,56,57,58,59,60,61,-1,-1,-1, 0,-1,-1,
        -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,
        15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,
        -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,
        41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,
    };

    size_t outLen = 0;
    uint32_t acc = 0;
    int bits = 0;

    for (size_t i = 0; input[i] != '\0'; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c == '=') break;
        int val = lut[c];
        if (val < 0) return 0;  // invalid character
        acc = (acc << 6) | (uint32_t)val;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (outLen >= maxLen) return 0;  // buffer overflow
            output[outLen++] = (uint8_t)(acc >> bits);
        }
    }
    return outLen;
}

static uint32_t crc32_le(uint32_t crc, const uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1U));
        }
    }
    return crc;
}

// =============================================================================
// base64Decode tests
// =============================================================================

void test_base64_decode_valid_three_bytes() {
    // "AAEC" in base64 = bytes 0x00, 0x01, 0x02
    uint8_t out[4];
    size_t n = base64Decode("AAEC", out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(3, n);
    TEST_ASSERT_EQUAL_HEX8(0x00, out[0]);
    TEST_ASSERT_EQUAL_HEX8(0x01, out[1]);
    TEST_ASSERT_EQUAL_HEX8(0x02, out[2]);
}

void test_base64_decode_hello() {
    // "SGVsbG8=" = "Hello"
    uint8_t out[8];
    size_t n = base64Decode("SGVsbG8=", out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(5, n);
    TEST_ASSERT_EQUAL_UINT8('H', out[0]);
    TEST_ASSERT_EQUAL_UINT8('e', out[1]);
    TEST_ASSERT_EQUAL_UINT8('l', out[2]);
    TEST_ASSERT_EQUAL_UINT8('l', out[3]);
    TEST_ASSERT_EQUAL_UINT8('o', out[4]);
}

void test_base64_decode_empty_input() {
    uint8_t out[4];
    size_t n = base64Decode("", out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(0, n);
}

void test_base64_decode_invalid_character() {
    uint8_t out[4];
    size_t n = base64Decode("!@#$", out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(0, n);
}

void test_base64_decode_buffer_overflow_returns_zero() {
    // "SGVsbG8=" = 5 bytes, but maxLen=3 → should return 0
    uint8_t out[3];
    size_t n = base64Decode("SGVsbG8=", out, 3);
    TEST_ASSERT_EQUAL_size_t(0, n);
}

void test_base64_decode_roundtrip_180_bytes() {
    // 240 base64 chars should decode to exactly 180 bytes
    // "AAAA" repeated 60 times = 60*3 = 180 zero bytes
    const char* b64 = "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
                      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA";
    uint8_t out[256];
    size_t n = base64Decode(b64, out, sizeof(out));
    TEST_ASSERT_EQUAL_size_t(180, n);
    for (size_t i = 0; i < n; i++) {
        TEST_ASSERT_EQUAL_HEX8(0x00, out[i]);
    }
}

// =============================================================================
// CRC32 tests
// =============================================================================

void test_crc32_known_reference_vector() {
    // CRC32 of "123456789" = 0xCBF43926 (ISO 3309 reference vector)
    const uint8_t data[] = {'1','2','3','4','5','6','7','8','9'};
    uint32_t crc = crc32_le(0, data, sizeof(data));
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926U, crc);
}

void test_crc32_empty_input() {
    uint32_t crc = crc32_le(0, nullptr, 0);
    TEST_ASSERT_EQUAL_HEX32(0x00000000U, crc);
}

void test_crc32_single_byte() {
    // CRC32 of {0x00} = 0xD202EF8D
    const uint8_t data[] = {0x00};
    uint32_t crc = crc32_le(0, data, 1);
    TEST_ASSERT_EQUAL_HEX32(0xD202EF8DU, crc);
}

void test_crc32_chained_matches_single_call() {
    // crc32(crc32(a), b) == crc32(a+b)
    const uint8_t a[] = {0x01, 0x02, 0x03};
    const uint8_t b[] = {0x04, 0x05, 0x06};
    const uint8_t ab[] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06};
    uint32_t chained = crc32_le(crc32_le(0, a, sizeof(a)), b, sizeof(b));
    uint32_t single  = crc32_le(0, ab, sizeof(ab));
    TEST_ASSERT_EQUAL_HEX32(single, chained);
}

void test_crc32_mismatch_detected() {
    const uint8_t data[]       = {0xDE, 0xAD, 0xBE, 0xEF};
    const uint8_t corrupted[]  = {0xDE, 0xAD, 0xBE, 0xEE};  // last byte flipped
    uint32_t crc_good = crc32_le(0, data, sizeof(data));
    uint32_t crc_bad  = crc32_le(0, corrupted, sizeof(corrupted));
    TEST_ASSERT_NOT_EQUAL(crc_good, crc_bad);
}

// =============================================================================
// ENTRY POINT
// =============================================================================

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    UNITY_BEGIN();

    RUN_TEST(test_base64_decode_valid_three_bytes);
    RUN_TEST(test_base64_decode_hello);
    RUN_TEST(test_base64_decode_empty_input);
    RUN_TEST(test_base64_decode_invalid_character);
    RUN_TEST(test_base64_decode_buffer_overflow_returns_zero);
    RUN_TEST(test_base64_decode_roundtrip_180_bytes);

    RUN_TEST(test_crc32_known_reference_vector);
    RUN_TEST(test_crc32_empty_input);
    RUN_TEST(test_crc32_single_byte);
    RUN_TEST(test_crc32_chained_matches_single_call);
    RUN_TEST(test_crc32_mismatch_detected);

    return UNITY_END();
}
