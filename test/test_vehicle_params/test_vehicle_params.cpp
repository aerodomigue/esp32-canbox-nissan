/**
 * @file test_vehicle_params.cpp
 * @brief Unit tests for vehicleParams loading logic in CanConfigProcessor
 *
 * Tests run natively on the host (no ESP32 required) using PlatformIO native env.
 * LittleFS is mocked to read from test/fixtures/. ConfigManager is fully stubbed.
 *
 * Run: pio test -e native
 */

#include <unity.h>
#include "CanConfigProcessor.h"
#include "ConfigManager_mock.h"
#include "LittleFS.h"
#include "GlobalData.h"
#include <string>

// Globals are defined in GlobalData_stub.cpp

void setUp() {
    mockReset();
    // Point LittleFS mock at the test fixtures directory
    LittleFS.basePath = "test/fixtures";
}

void tearDown() {
    remove("test/fixtures/.invalid_vehicle_params.json");
}

// =============================================================================
// FIRST BOOT: no saved vehicle → vehicle switch path triggered
// =============================================================================

void test_first_boot_applies_all_vehicle_params() {
    // NVS is empty on first boot
    g_mock.vehicleFile[0] = '\0';

    CanConfigProcessor proc;
    proc.loadFromJson("/full_params.json");

    TEST_ASSERT_EQUAL_INT(1, g_mock.resetCount);
    TEST_ASSERT_EQUAL_INT(1, g_mock.saveCount);
    TEST_ASSERT_EQUAL_UINT16(500, g_mock.steerScale);
    TEST_ASSERT_EQUAL_INT16(50, g_mock.steerOffset);
    TEST_ASSERT_FALSE(g_mock.steerInvert);
    TEST_ASSERT_EQUAL_UINT16(800, g_mock.indicatorTimeout);
    TEST_ASSERT_EQUAL_UINT8(4, g_mock.rpmDivisor);
    TEST_ASSERT_EQUAL_UINT8(60, g_mock.tankCapacity);
    TEST_ASSERT_EQUAL_UINT16(200, g_mock.dteDivisor);
    TEST_ASSERT_TRUE(g_mock.steerScaleSet);
    TEST_ASSERT_TRUE(g_mock.steerOffsetSet);
    TEST_ASSERT_TRUE(g_mock.steerInvertSet);
    TEST_ASSERT_EQUAL_STRING("full_params.json", g_mock.vehicleFile);
}

// =============================================================================
// SAME VEHICLE: explicit JSON parameters win over local calibration
// =============================================================================

void test_same_vehicle_reapplies_json_values() {
    // Simulate user has saved custom steerScale
    strncpy(g_mock.vehicleFile, "full_params.json", sizeof(g_mock.vehicleFile));
    g_mock.steerScale = 9999;
    g_mock.steerScaleSet = true;

    CanConfigProcessor proc;
    proc.loadFromJson("/full_params.json");

    TEST_ASSERT_EQUAL_INT(0, g_mock.resetCount);
    TEST_ASSERT_EQUAL_INT(1, g_mock.saveCount);
    TEST_ASSERT_EQUAL_UINT16(500, g_mock.steerScale);
    TEST_ASSERT_EQUAL_INT16(50, g_mock.steerOffset);
    TEST_ASSERT_FALSE(g_mock.steerInvert);
    TEST_ASSERT_EQUAL_UINT16(800, g_mock.indicatorTimeout);
    TEST_ASSERT_EQUAL_UINT8(4, g_mock.rpmDivisor);
    TEST_ASSERT_EQUAL_UINT8(60, g_mock.tankCapacity);
    TEST_ASSERT_EQUAL_UINT16(200, g_mock.dteDivisor);
}

void test_unchanged_reload_does_not_save_calibration_again() {
    CanConfigProcessor proc;
    TEST_ASSERT_TRUE(proc.loadFromJson("/full_params.json"));
    g_mock.resetCount = g_mock.saveCount = 0;
    TEST_ASSERT_TRUE(proc.loadFromJson("/full_params.json"));
    TEST_ASSERT_EQUAL_INT(0, g_mock.resetCount);
    TEST_ASSERT_EQUAL_INT(0, g_mock.saveCount);
}

void test_begin_restores_json_over_saved_local_values() {
    strncpy(g_mock.vehicleFile, "full_params.json", sizeof(g_mock.vehicleFile));
    g_mock.steerScale = 9999;
    CanConfigProcessor proc;
    TEST_ASSERT_TRUE(proc.begin());
    TEST_ASSERT_EQUAL_STRING("Test Vehicle Full", proc.getProfileName());
    TEST_ASSERT_EQUAL_UINT16(500, g_mock.steerScale);
    TEST_ASSERT_EQUAL_INT(0, g_mock.resetCount);
    TEST_ASSERT_EQUAL_INT(1, g_mock.saveCount);
}

void test_partial_reload_preserves_null_overrides() {
    CanConfigProcessor proc;
    TEST_ASSERT_TRUE(proc.loadFromJson("/partial_params.json"));
    g_mock.steerScale = 9999;
    g_mock.steerOffset = -123;
    g_mock.steerInvert = false;
    g_mock.indicatorTimeout = 777;
    g_mock.rpmDivisor = 8;
    g_mock.tankCapacity = 60;
    g_mock.dteDivisor = 338;
    g_mock.resetCount = g_mock.saveCount = 0;
    TEST_ASSERT_TRUE(proc.loadFromJson("/partial_params.json"));
    TEST_ASSERT_EQUAL_UINT16(200, g_mock.steerScale);
    TEST_ASSERT_EQUAL_INT16(-123, g_mock.steerOffset);
    TEST_ASSERT_FALSE(g_mock.steerInvert);
    TEST_ASSERT_EQUAL_UINT16(777, g_mock.indicatorTimeout);
    TEST_ASSERT_EQUAL_UINT8(8, g_mock.rpmDivisor);
    TEST_ASSERT_EQUAL_UINT8(60, g_mock.tankCapacity);
    TEST_ASSERT_EQUAL_UINT16(338, g_mock.dteDivisor);
    TEST_ASSERT_EQUAL_INT(0, g_mock.resetCount);
    TEST_ASSERT_EQUAL_INT(1, g_mock.saveCount);
}

void test_reload_without_overrides_preserves_local_values() {
    for (const char* path : {"/no_params.json", "/empty_params.json"}) {
        CanConfigProcessor proc;
        TEST_ASSERT_TRUE(proc.loadFromJson(path));
        g_mock.steerScale = 9999;
        g_mock.resetCount = g_mock.saveCount = 0;
        TEST_ASSERT_TRUE(proc.loadFromJson(path));
        TEST_ASSERT_EQUAL_UINT16(9999, g_mock.steerScale);
        TEST_ASSERT_EQUAL_INT(0, g_mock.resetCount);
        TEST_ASSERT_EQUAL_INT(0, g_mock.saveCount);
    }
}

static bool loadParams(CanConfigProcessor& proc, const char* params) {
    File file = LittleFS.open("/.invalid_vehicle_params.json", "w");
    const std::string json = std::string("{\"name\":\"Candidate\",\"isMock\":true,\"vehicleParams\":") + params + "}";
    TEST_ASSERT_TRUE(file);
    TEST_ASSERT_EQUAL_UINT32(json.size(), file.write(reinterpret_cast<const uint8_t*>(json.data()), json.size()));
    file.close();
    return proc.loadFromJson("/.invalid_vehicle_params.json");
}

void test_invalid_overrides_keep_active_profile_and_calibration() {
    const char* invalid[] = {
        "[]", "false", "42", "\"bad\"",
        "{\"steerScale\":0}", "{\"steerScale\":20001}", "{\"steerScale\":-1}",
        "{\"steerScale\":65537}", "{\"steerScale\":4294967297}",
        "{\"steerScale\":500.5}", "{\"steerScale\":\"500\"}", "{\"steerScale\":true}",
        "{\"steerOffset\":-501}", "{\"steerOffset\":501}",
        "{\"steerInvert\":1}", "{\"steerInvert\":\"false\"}",
        "{\"indTimeout\":99}", "{\"indTimeout\":2001}",
        "{\"rpmDiv\":0}", "{\"rpmDiv\":21}",
        "{\"tankCap\":19}", "{\"tankCap\":101}",
        "{\"dteDiv\":99}", "{\"dteDiv\":501}",
        "{\"steerScale\":600,\"dteDiv\":0}"
    };
    CanConfigProcessor proc;
    TEST_ASSERT_TRUE(proc.loadFromJson("/full_params.json"));
    canAuxData.handbrakeValid = true;
    g_mock.resetCount = g_mock.saveCount = 0;
    for (const char* params : invalid) {
        TEST_ASSERT_FALSE_MESSAGE(loadParams(proc, params), params);
        TEST_ASSERT_EQUAL_STRING("Test Vehicle Full", proc.getProfileName());
        TEST_ASSERT_FALSE(proc.isMockMode());
        TEST_ASSERT_TRUE(canAuxData.handbrakeValid);
        TEST_ASSERT_EQUAL_STRING("full_params.json", g_mock.vehicleFile);
        TEST_ASSERT_EQUAL_UINT16(500, g_mock.steerScale);
        TEST_ASSERT_EQUAL_UINT16(200, g_mock.dteDivisor);
        TEST_ASSERT_EQUAL_INT(0, g_mock.resetCount);
        TEST_ASSERT_EQUAL_INT(0, g_mock.saveCount);
    }
}

void test_boundary_and_null_overrides_are_accepted() {
    CanConfigProcessor proc;
    TEST_ASSERT_TRUE(loadParams(proc, "{\"steerScale\":1,\"steerOffset\":-500,\"steerInvert\":false,\"indTimeout\":100,\"rpmDiv\":1,\"tankCap\":20,\"dteDiv\":100}"));
    TEST_ASSERT_EQUAL_UINT16(1, g_mock.steerScale);
    TEST_ASSERT_EQUAL_INT16(-500, g_mock.steerOffset);
    TEST_ASSERT_FALSE(g_mock.steerInvert);
    TEST_ASSERT_TRUE(loadParams(proc, "{\"steerScale\":20000,\"steerOffset\":500,\"steerInvert\":true,\"indTimeout\":2000,\"rpmDiv\":20,\"tankCap\":100,\"dteDiv\":500}"));
    TEST_ASSERT_EQUAL_UINT16(20000, g_mock.steerScale);
    TEST_ASSERT_EQUAL_INT16(500, g_mock.steerOffset);
    TEST_ASSERT_TRUE(g_mock.steerInvert);
    TEST_ASSERT_TRUE(loadParams(proc, "null"));
    TEST_ASSERT_EQUAL_UINT16(20000, g_mock.steerScale);
}

// =============================================================================
// VEHICLE SWITCH: different file → reset + apply new vehicle params
// =============================================================================

void test_vehicle_switch_resets_then_applies() {
    // Loaded from a different vehicle before
    strncpy(g_mock.vehicleFile, "other_vehicle.json", sizeof(g_mock.vehicleFile));
    g_mock.steerScale = 9999;

    CanConfigProcessor proc;
    proc.loadFromJson("/full_params.json");

    TEST_ASSERT_EQUAL_INT(1, g_mock.resetCount);
    TEST_ASSERT_EQUAL_INT(1, g_mock.saveCount);
    TEST_ASSERT_EQUAL_UINT16(500, g_mock.steerScale);  // vehicle params applied
    TEST_ASSERT_EQUAL_STRING("full_params.json", g_mock.vehicleFile);
}

// =============================================================================
// PARTIAL PARAMS: only non-null fields applied
// =============================================================================

void test_null_fields_are_not_applied() {
    g_mock.vehicleFile[0] = '\0';
    // Set some initial values that should survive null fields
    g_mock.steerOffset = 150;
    g_mock.steerInvert = true;

    CanConfigProcessor proc;
    proc.loadFromJson("/partial_params.json");

    TEST_ASSERT_EQUAL_INT(1, g_mock.resetCount);
    TEST_ASSERT_EQUAL_UINT16(200, g_mock.steerScale);   // applied (non-null)
    TEST_ASSERT_TRUE(g_mock.steerScaleSet);
    TEST_ASSERT_FALSE(g_mock.steerOffsetSet);           // null → not applied
    TEST_ASSERT_FALSE(g_mock.steerInvertSet);           // null → not applied
    TEST_ASSERT_FALSE(g_mock.rpmDivisorSet);            // null → not applied
}

// =============================================================================
// NO VEHICLEPARAMS KEY: vehicle switch still resets NVS, no params applied
// =============================================================================

void test_absent_vehicle_params_resets_but_no_params_applied() {
    strncpy(g_mock.vehicleFile, "other_vehicle.json", sizeof(g_mock.vehicleFile));
    g_mock.steerScale = 9999;

    CanConfigProcessor proc;
    proc.loadFromJson("/no_params.json");

    TEST_ASSERT_EQUAL_INT(1, g_mock.resetCount);    // reset always on switch
    TEST_ASSERT_EQUAL_INT(1, g_mock.saveCount);
    TEST_ASSERT_FALSE(g_mock.steerScaleSet);        // no params in JSON → not applied
    // After configReset(), steerScale is 300 (firmware default from mock)
    TEST_ASSERT_EQUAL_UINT16(300, g_mock.steerScale);
}

// =============================================================================
// EMPTY VEHICLEPARAMS: object present but empty → reset, zero overrides
// =============================================================================

void test_empty_vehicle_params_resets_but_no_params_applied() {
    g_mock.vehicleFile[0] = '\0';

    CanConfigProcessor proc;
    proc.loadFromJson("/empty_params.json");

    TEST_ASSERT_EQUAL_INT(1, g_mock.resetCount);
    TEST_ASSERT_EQUAL_INT(1, g_mock.saveCount);
    TEST_ASSERT_FALSE(g_mock.steerScaleSet);
    TEST_ASSERT_FALSE(g_mock.steerOffsetSet);
    TEST_ASSERT_FALSE(g_mock.steerInvertSet);
}

// =============================================================================
// PROFILE NAME: loadFromJson populates profile name
// =============================================================================

void test_profile_name_loaded_correctly() {
    g_mock.vehicleFile[0] = '\0';

    CanConfigProcessor proc;
    proc.loadFromJson("/full_params.json");

    TEST_ASSERT_EQUAL_STRING("Test Vehicle Full", proc.getProfileName());
}

// =============================================================================
// MOCK MODE: isMock flag respected
// =============================================================================

void test_mock_mode_flag_from_json() {
    g_mock.vehicleFile[0] = '\0';

    CanConfigProcessor proc;
    proc.loadFromJson("/empty_params.json");

    TEST_ASSERT_TRUE(proc.isMockMode());
}

void test_real_can_mode_flag_from_json() {
    g_mock.vehicleFile[0] = '\0';

    CanConfigProcessor proc;
    proc.loadFromJson("/full_params.json");

    TEST_ASSERT_FALSE(proc.isMockMode());
}

// =============================================================================
// MAIN
// =============================================================================

int main(int argc, char** argv) {
    UNITY_BEGIN();

    RUN_TEST(test_first_boot_applies_all_vehicle_params);
    RUN_TEST(test_same_vehicle_reapplies_json_values);
    RUN_TEST(test_unchanged_reload_does_not_save_calibration_again);
    RUN_TEST(test_begin_restores_json_over_saved_local_values);
    RUN_TEST(test_partial_reload_preserves_null_overrides);
    RUN_TEST(test_reload_without_overrides_preserves_local_values);
    RUN_TEST(test_invalid_overrides_keep_active_profile_and_calibration);
    RUN_TEST(test_boundary_and_null_overrides_are_accepted);
    RUN_TEST(test_vehicle_switch_resets_then_applies);
    RUN_TEST(test_null_fields_are_not_applied);
    RUN_TEST(test_absent_vehicle_params_resets_but_no_params_applied);
    RUN_TEST(test_empty_vehicle_params_resets_but_no_params_applied);
    RUN_TEST(test_profile_name_loaded_correctly);
    RUN_TEST(test_mock_mode_flag_from_json);
    RUN_TEST(test_real_can_mode_flag_from_json);

    return UNITY_END();
}
