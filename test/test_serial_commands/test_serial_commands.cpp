/**
 * @file test_serial_commands.cpp
 * @brief Unit tests for SerialCommand — parsing, CFG, LOG, SYS, CAN, OTA
 *
 * Uses testProcessCommand() (UNIT_TEST accessor) to inject commands directly
 * without needing a real serial port. State is verified via ConfigManager
 * mock and isXxx() public APIs.
 *
 * Run: pio test -e native_serial
 */

#include <unity.h>
#include "SerialCommand.h"
#include "ConfigManager_mock.h"
#include "LittleFS.h"

void setUp() {
    mockReset();
    LittleFS.basePath = "data";
    // Reset static state in SerialCommand (canLogEnabled persists across tests)
    testProcessCommand("LOG OFF");
}

void tearDown() {}

// =============================================================================
// PARSING — routing, case insensitivity, edge cases
// =============================================================================

void test_empty_command_does_not_crash() {
    testProcessCommand("");
    testProcessCommand("   ");
    // No assert needed — just must not crash
    TEST_PASS();
}

void test_unknown_command_does_not_crash() {
    testProcessCommand("FOOBAR");
    testProcessCommand("INVALID_CMD 123 456");
    TEST_PASS();
}

void test_command_case_insensitive() {
    testProcessCommand("log on");
    TEST_ASSERT_TRUE(isCanLogEnabled());
    testProcessCommand("LOG OFF");
    TEST_ASSERT_FALSE(isCanLogEnabled());
    testProcessCommand("Log On");
    TEST_ASSERT_TRUE(isCanLogEnabled());
}

void test_command_with_leading_spaces() {
    testProcessCommand("  LOG ON");
    TEST_ASSERT_TRUE(isCanLogEnabled());
}

// =============================================================================
// CFG — calibration parameter get/set/list/reset/save
// =============================================================================

void test_cfg_set_steer_scale() {
    testProcessCommand("CFG SET steerScale 500");
    TEST_ASSERT_EQUAL_UINT16(500, g_mock.steerScale);
    TEST_ASSERT_TRUE(g_mock.steerScaleSet);
}

void test_cfg_set_steer_offset() {
    testProcessCommand("CFG SET steerOffset -50");
    TEST_ASSERT_EQUAL_INT16(-50, g_mock.steerOffset);
    TEST_ASSERT_TRUE(g_mock.steerOffsetSet);
}

void test_cfg_set_steer_invert_true() {
    testProcessCommand("CFG SET steerInvert 1");
    TEST_ASSERT_TRUE(g_mock.steerInvert);
    TEST_ASSERT_TRUE(g_mock.steerInvertSet);
}

void test_cfg_set_steer_invert_false() {
    g_mock.steerInvert = true;
    testProcessCommand("CFG SET steerInvert 0");
    TEST_ASSERT_FALSE(g_mock.steerInvert);
}

void test_cfg_set_ind_timeout() {
    testProcessCommand("CFG SET indTimeout 800");
    TEST_ASSERT_EQUAL_UINT16(800, g_mock.indicatorTimeout);
    TEST_ASSERT_TRUE(g_mock.indicatorTimeoutSet);
}

void test_cfg_set_tank_cap() {
    testProcessCommand("CFG SET tankCap 60");
    TEST_ASSERT_EQUAL_UINT8(60, g_mock.tankCapacity);
    TEST_ASSERT_TRUE(g_mock.tankCapacitySet);
}

void test_cfg_set_rpm_div() {
    testProcessCommand("CFG SET rpmDiv 8");
    TEST_ASSERT_EQUAL_UINT8(8, g_mock.rpmDivisor);
    TEST_ASSERT_TRUE(g_mock.rpmDivisorSet);
}

void test_cfg_set_dte_div() {
    testProcessCommand("CFG SET dteDiv 338");
    TEST_ASSERT_EQUAL_UINT16(338, g_mock.dteDivisor);
    TEST_ASSERT_TRUE(g_mock.dteDivisorSet);
}

void test_cfg_set_unknown_param_does_not_crash() {
    testProcessCommand("CFG SET nonExistent 999");
    TEST_PASS();
}

void test_cfg_reset_applies_defaults() {
    g_mock.steerScale = 9999;
    testProcessCommand("CFG RESET");
    // configReset() sets steerScale=300 in the stub — verify the effect
    TEST_ASSERT_EQUAL_UINT16(300, g_mock.steerScale);
}

void test_cfg_save_calls_save() {
    testProcessCommand("CFG SAVE");
    TEST_ASSERT_EQUAL_INT(1, g_mock.saveCount);
}

void test_cfg_get_does_not_crash() {
    testProcessCommand("CFG GET steerScale");
    testProcessCommand("CFG GET steerOffset");
    testProcessCommand("CFG GET steerInvert");
    testProcessCommand("CFG GET indTimeout");
    testProcessCommand("CFG GET tankCap");
    testProcessCommand("CFG GET rpmDiv");
    testProcessCommand("CFG GET dteDiv");
    TEST_PASS();
}

void test_cfg_list_does_not_crash() {
    testProcessCommand("CFG LIST");
    TEST_PASS();
}

// =============================================================================
// LOG — CAN frame logging toggle
// =============================================================================

void test_log_on_enables_logging() {
    TEST_ASSERT_FALSE(isCanLogEnabled());
    testProcessCommand("LOG ON");
    TEST_ASSERT_TRUE(isCanLogEnabled());
}

void test_log_off_disables_logging() {
    testProcessCommand("LOG ON");
    testProcessCommand("LOG OFF");
    TEST_ASSERT_FALSE(isCanLogEnabled());
}

void test_log_toggle_multiple_times() {
    for (int i = 0; i < 3; i++) {
        testProcessCommand("LOG ON");
        TEST_ASSERT_TRUE(isCanLogEnabled());
        testProcessCommand("LOG OFF");
        TEST_ASSERT_FALSE(isCanLogEnabled());
    }
}

void test_log_unknown_subcommand_does_not_crash() {
    testProcessCommand("LOG INVALID");
    TEST_PASS();
}

// =============================================================================
// SYS — system info/data/reset
// =============================================================================

void test_sys_info_does_not_crash() {
    testProcessCommand("SYS INFO");
    TEST_PASS();
}

void test_sys_data_does_not_crash() {
    testProcessCommand("SYS DATA");
    TEST_PASS();
}

void test_sys_unknown_subcommand_does_not_crash() {
    testProcessCommand("SYS UNKNOWN");
    TEST_PASS();
}

// =============================================================================
// CAN — config file management
// =============================================================================

void test_can_status_does_not_crash() {
    testProcessCommand("CAN STATUS");
    TEST_PASS();
}

void test_can_list_does_not_crash() {
    testProcessCommand("CAN LIST");
    TEST_PASS();
}

void test_can_get_does_not_crash() {
    testProcessCommand("CAN GET");
    TEST_PASS();
}

void test_can_load_missing_file_does_not_crash() {
    testProcessCommand("CAN LOAD nonexistent.json");
    TEST_PASS();
}

void test_can_reload_does_not_crash() {
    testProcessCommand("CAN RELOAD");
    TEST_PASS();
}

// =============================================================================
// OTA — firmware update protocol
// =============================================================================

void test_ota_status_not_in_progress_initially() {
    TEST_ASSERT_FALSE(isOtaInProgress());
}

void test_ota_status_does_not_crash() {
    testProcessCommand("OTA STATUS");
    TEST_PASS();
}

void test_ota_abort_when_not_active_does_not_crash() {
    testProcessCommand("OTA ABORT");
    TEST_ASSERT_FALSE(isOtaInProgress());
}

void test_ota_unknown_subcommand_does_not_crash() {
    testProcessCommand("OTA UNKNOWN");
    TEST_PASS();
}

// =============================================================================
// HELP
// =============================================================================

void test_help_does_not_crash() {
    testProcessCommand("HELP");
    testProcessCommand("?");
    TEST_PASS();
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    // Parsing
    RUN_TEST(test_empty_command_does_not_crash);
    RUN_TEST(test_unknown_command_does_not_crash);
    RUN_TEST(test_command_case_insensitive);
    RUN_TEST(test_command_with_leading_spaces);

    // CFG
    RUN_TEST(test_cfg_set_steer_scale);
    RUN_TEST(test_cfg_set_steer_offset);
    RUN_TEST(test_cfg_set_steer_invert_true);
    RUN_TEST(test_cfg_set_steer_invert_false);
    RUN_TEST(test_cfg_set_ind_timeout);
    RUN_TEST(test_cfg_set_tank_cap);
    RUN_TEST(test_cfg_set_rpm_div);
    RUN_TEST(test_cfg_set_dte_div);
    RUN_TEST(test_cfg_set_unknown_param_does_not_crash);
    RUN_TEST(test_cfg_reset_applies_defaults);
    RUN_TEST(test_cfg_save_calls_save);
    RUN_TEST(test_cfg_get_does_not_crash);
    RUN_TEST(test_cfg_list_does_not_crash);

    // LOG
    RUN_TEST(test_log_on_enables_logging);
    RUN_TEST(test_log_off_disables_logging);
    RUN_TEST(test_log_toggle_multiple_times);
    RUN_TEST(test_log_unknown_subcommand_does_not_crash);

    // SYS
    RUN_TEST(test_sys_info_does_not_crash);
    RUN_TEST(test_sys_data_does_not_crash);
    RUN_TEST(test_sys_unknown_subcommand_does_not_crash);

    // CAN
    RUN_TEST(test_can_status_does_not_crash);
    RUN_TEST(test_can_list_does_not_crash);
    RUN_TEST(test_can_get_does_not_crash);
    RUN_TEST(test_can_load_missing_file_does_not_crash);
    RUN_TEST(test_can_reload_does_not_crash);

    // OTA
    RUN_TEST(test_ota_status_not_in_progress_initially);
    RUN_TEST(test_ota_status_does_not_crash);
    RUN_TEST(test_ota_abort_when_not_active_does_not_crash);
    RUN_TEST(test_ota_unknown_subcommand_does_not_crash);

    // HELP
    RUN_TEST(test_help_does_not_crash);

    return UNITY_END();
}
