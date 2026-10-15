#include <unity.h>
#include "FuelConsumptionEstimator.h"

#include <cmath>
#include <limits>

void setUp() {}
void tearDown() {}

namespace {
FuelConsumptionConfig enabledConfig() {
    FuelConsumptionConfig config;
    config.enabled = true;
    return config;
}

void closeTo(double expected, double actual, double tolerance = 1e-7) {
    TEST_ASSERT_TRUE_MESSAGE(std::fabs(expected - actual) <= tolerance,
                             "Unexpected physical fuel volume or distance");
}

void start(FuelConsumptionEstimator& estimator, double speed,
           uint8_t counter = 0, uint32_t now = 0) {
    estimator.onSpeed(speed, now);
    estimator.onCounter(counter, now);
}

// 100 ms source periods; one increment per period means 10 ticks/second.
void drive(FuelConsumptionEstimator& estimator, double speed,
           uint32_t startAt, unsigned samples, uint8_t startCounter = 0,
           uint8_t ticksPerSample = 1) {
    for (unsigned i = 1; i <= samples; ++i) {
        const uint32_t now = startAt + i * 100u;
        estimator.onSpeed(speed, now);
        estimator.onCounter(static_cast<uint8_t>(startCounter + i * ticksPerSample), now);
    }
}
}

void test_config_rejects_nonfinite_invalid_ranges_and_ambiguous_full_wrap() {
    FuelConsumptionConfig config = enabledConfig();
    TEST_ASSERT_TRUE(config.isValid());
    config.mm3PerTick = std::numeric_limits<double>::quiet_NaN();
    TEST_ASSERT_FALSE(config.isValid());
    config.mm3PerTick = std::numeric_limits<double>::infinity();
    TEST_ASSERT_FALSE(config.isValid());
    config.mm3PerTick = 0;
    TEST_ASSERT_FALSE(config.isValid());
    config.mm3PerTick = -80;
    TEST_ASSERT_FALSE(config.isValid());
    config = enabledConfig();
    config.windowMs = 99;
    TEST_ASSERT_FALSE(config.isValid());
    config.windowMs = 10001;
    TEST_ASSERT_FALSE(config.isValid());
    config = enabledConfig();
    config.maxGapMs = 99;
    TEST_ASSERT_FALSE(config.isValid());
    config.maxGapMs = 1001;
    TEST_ASSERT_FALSE(config.isValid());
    config = enabledConfig();
    config.minSpeedKmh = -0.01;
    TEST_ASSERT_FALSE(config.isValid());
    config.minSpeedKmh = std::numeric_limits<double>::quiet_NaN();
    TEST_ASSERT_FALSE(config.isValid());
    config = enabledConfig();
    config.minAverageDistanceM = 0;
    TEST_ASSERT_FALSE(config.isValid());
    config.minAverageDistanceM = std::numeric_limits<double>::infinity();
    TEST_ASSERT_FALSE(config.isValid());
    config = enabledConfig();
    config.maxTicksPerSecond = 0;
    TEST_ASSERT_FALSE(config.isValid());
    config.maxTicksPerSecond = 510; // 510 * 0.5 + 1 = 256: ambiguous.
    TEST_ASSERT_FALSE(config.isValid());
    config.maxTicksPerSecond = 509.9;
    TEST_ASSERT_TRUE(config.isValid());
}

void test_disabled_and_invalid_configs_never_produce_measurements() {
    FuelConsumptionEstimator estimator;
    start(estimator, 36.0);
    drive(estimator, 36.0, 0, 120);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    TEST_ASSERT_FALSE(estimator.averageValid());
    closeTo(0, estimator.totalFuelMm3());
    closeTo(0, estimator.totalDistanceM());
    auto config = enabledConfig();
    config.mm3PerTick = -1;
    TEST_ASSERT_FALSE(estimator.configure(config));
    start(estimator, 36.0);
    drive(estimator, 36.0, 0, 120);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    TEST_ASSERT_FALSE(estimator.averageValid());
}

void test_known_constant_trip_converts_volume_and_distance_to_radio_units() {
    FuelConsumptionEstimator estimator;
    TEST_ASSERT_TRUE(estimator.configure(enabledConfig()));
    start(estimator, 36.0, 203);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    drive(estimator, 36.0, 0, 120, 203);
    // 12 seconds at 10 m/s, consuming 0.8 ml/s => 8.0 L/100km.
    closeTo(120, estimator.totalDistanceM());
    closeTo(9600, estimator.totalFuelMm3());
    TEST_ASSERT_TRUE(estimator.instantaneousValid());
    TEST_ASSERT_EQUAL_UINT16(80, estimator.instantaneous());
    TEST_ASSERT_TRUE(estimator.averageValid());
    TEST_ASSERT_EQUAL_UINT16(80, estimator.average());
}

void test_fractional_speed_survives_distance_integration_and_rounding() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36.50);
    drive(estimator, 36.50, 0, 100);
    closeTo(36.50 / 3.6 * 10, estimator.totalDistanceM());
    TEST_ASSERT_EQUAL_UINT16(79, estimator.instantaneous());
    TEST_ASSERT_EQUAL_UINT16(79, estimator.average());
    TEST_ASSERT_TRUE(estimator.averageValid());

    auto config = enabledConfig();
    config.minSpeedKmh = 0.0;
    config.minAverageDistanceM = 0.001;
    estimator.configure(config);
    start(estimator, 0.01);
    drive(estimator, 0.01, 0, 10);
    closeTo(0.01 / 3.6, estimator.totalDistanceM());
    TEST_ASSERT_TRUE(estimator.averageValid());
}

void test_counter_rollover_counts_only_modulo_difference() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36, 250);
    estimator.onSpeed(36, 100);
    estimator.onCounter(254, 100);
    estimator.onSpeed(36, 200);
    estimator.onCounter(1, 200);
    closeTo(7 * 80, estimator.totalFuelMm3());
    closeTo(2, estimator.totalDistanceM());
}

void test_initial_counter_and_unsynchronized_start_do_not_invent_consumption() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    estimator.onCounter(200, 0);
    estimator.onSpeed(36, 50);
    estimator.onCounter(202, 100);
    closeTo(0, estimator.totalFuelMm3());
    closeTo(0, estimator.totalDistanceM());
    estimator.onSpeed(36, 150);
    estimator.onCounter(203, 200);
    closeTo(80, estimator.totalFuelMm3());
    closeTo(1, estimator.totalDistanceM());
}

void test_average_waits_for_distance_threshold() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36);
    drive(estimator, 36, 0, 99);
    closeTo(99, estimator.totalDistanceM());
    TEST_ASSERT_FALSE(estimator.averageValid());
    estimator.onSpeed(36, 10000);
    estimator.onCounter(100, 10000);
    TEST_ASSERT_TRUE(estimator.averageValid());
    TEST_ASSERT_EQUAL_UINT16(80, estimator.average());
}

void test_stopped_display_is_zero_but_idle_fuel_increases_session_average() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36);
    drive(estimator, 36, 0, 100);
    estimator.onSpeed(0, 10000);
    TEST_ASSERT_TRUE(estimator.instantaneousValid());
    TEST_ASSERT_EQUAL_UINT16(0, estimator.instantaneous());
    drive(estimator, 0, 10000, 20, 100);
    closeTo(100, estimator.totalDistanceM());
    closeTo(9600, estimator.totalFuelMm3());
    TEST_ASSERT_TRUE(estimator.averageValid());
    TEST_ASSERT_EQUAL_UINT16(96, estimator.average());
    TEST_ASSERT_EQUAL_UINT16(0, estimator.instantaneous());
}

void test_starting_stationary_never_divides_by_zero_or_discards_idle_fuel() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 0);
    drive(estimator, 0, 0, 20);
    TEST_ASSERT_TRUE(estimator.instantaneousValid());
    TEST_ASSERT_EQUAL_UINT16(0, estimator.instantaneous());
    TEST_ASSERT_FALSE(estimator.averageValid());
    closeTo(1600, estimator.totalFuelMm3());
    estimator.onSpeed(36, 2000);
    drive(estimator, 36, 2000, 100, 20);
    closeTo(100, estimator.totalDistanceM());
    TEST_ASSERT_EQUAL_UINT16(96, estimator.average());
}

void test_fuel_cut_while_moving_is_a_valid_zero() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36, 77);
    drive(estimator, 36, 0, 100, 77, 0);
    TEST_ASSERT_TRUE(estimator.instantaneousValid());
    TEST_ASSERT_TRUE(estimator.averageValid());
    TEST_ASSERT_EQUAL_UINT16(0, estimator.instantaneous());
    TEST_ASSERT_EQUAL_UINT16(0, estimator.average());
    closeTo(100, estimator.totalDistanceM());
}

void test_below_display_speed_keeps_consumed_volume() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 1.99);
    drive(estimator, 1.99, 0, 10);
    TEST_ASSERT_TRUE(estimator.instantaneousValid());
    TEST_ASSERT_EQUAL_UINT16(0, estimator.instantaneous());
    closeTo(800, estimator.totalFuelMm3());
    closeTo(1.99 / 3.6, estimator.totalDistanceM());
}

void test_tick_invalidates_fuel_and_distance_when_entire_bus_falls_silent() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36);
    drive(estimator, 36, 0, 100);
    estimator.tick(10500); // Exactly the configured maximum gap is accepted.
    TEST_ASSERT_TRUE(estimator.instantaneousValid());
    estimator.tick(10501);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    TEST_ASSERT_FALSE(estimator.averageValid());
    closeTo(0, estimator.totalFuelMm3());
    closeTo(0, estimator.totalDistanceM());
}

void test_speed_loss_cannot_mix_partial_fuel_with_stale_distance() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36);
    drive(estimator, 36, 0, 100);
    for (unsigned i = 1; i <= 6; ++i) estimator.onCounter(100 + i, 10000 + i * 100);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    TEST_ASSERT_FALSE(estimator.averageValid());
    closeTo(0, estimator.totalFuelMm3());
    closeTo(0, estimator.totalDistanceM());
    estimator.onSpeed(36, 10650);
    estimator.onCounter(107, 10700); // New aligned baseline, no missing fuel.
    closeTo(0, estimator.totalFuelMm3());
    drive(estimator, 36, 10700, 10, 107);
    TEST_ASSERT_EQUAL_UINT16(80, estimator.instantaneous());
    closeTo(10, estimator.totalDistanceM());
}

void test_counter_loss_cannot_keep_distance_from_a_previous_session() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36);
    drive(estimator, 36, 0, 100);
    for (unsigned i = 1; i <= 6; ++i) estimator.onSpeed(36, 10000 + i * 100);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    TEST_ASSERT_FALSE(estimator.averageValid());
    closeTo(0, estimator.totalDistanceM());
    estimator.onCounter(200, 10600); // Unknown gap is not 100 consumed ticks.
    closeTo(0, estimator.totalFuelMm3());
    drive(estimator, 36, 10600, 10, 200);
    closeTo(800, estimator.totalFuelMm3());
    closeTo(10, estimator.totalDistanceM());
}

void test_implausible_counter_jump_rebases_without_fabricating_volume() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36, 100);
    estimator.onSpeed(36, 100);
    estimator.onCounter(90, 100); // Backwards jump => 246 ticks > budget.
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    TEST_ASSERT_FALSE(estimator.averageValid());
    closeTo(0, estimator.totalFuelMm3());
    closeTo(0, estimator.totalDistanceM());
    estimator.onSpeed(36, 200);
    estimator.onCounter(91, 200);
    drive(estimator, 36, 200, 10, 91);
    closeTo(800, estimator.totalFuelMm3());
    TEST_ASSERT_EQUAL_UINT16(80, estimator.instantaneous());
}

void test_queued_frames_with_identical_timestamps_do_not_lose_increments() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36, 0);
    estimator.onSpeed(36, 300);
    estimator.onCounter(1, 300);
    estimator.onCounter(2, 300);
    estimator.onCounter(3, 300);
    closeTo(240, estimator.totalFuelMm3());
    closeTo(3, estimator.totalDistanceM());
    drive(estimator, 36, 300, 7, 3);
    closeTo(800, estimator.totalFuelMm3());
    closeTo(10, estimator.totalDistanceM());
    TEST_ASSERT_EQUAL_UINT16(80, estimator.instantaneous());
}

void test_tick_budget_rejects_excessive_sustained_rate() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36, 0);
    estimator.onSpeed(36, 100);
    estimator.onCounter(41, 100); // 40 ticks plus one quantization tick.
    closeTo(41 * 80, estimator.totalFuelMm3());
    estimator.onSpeed(36, 200);
    estimator.onCounter(82, 200); // A further 41 cannot fit the 400 ticks/s rate.
    closeTo(0, estimator.totalFuelMm3());
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
}

void test_millisecond_wrap_preserves_elapsed_time_and_timeout() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    const uint32_t origin = std::numeric_limits<uint32_t>::max() - 550;
    start(estimator, 36, 250, origin);
    drive(estimator, 36, origin, 100, 250);
    closeTo(100, estimator.totalDistanceM());
    closeTo(8000, estimator.totalFuelMm3());
    TEST_ASSERT_EQUAL_UINT16(80, estimator.instantaneous());
    TEST_ASSERT_EQUAL_UINT16(80, estimator.average());
    estimator.tick(origin + 10501u);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    TEST_ASSERT_FALSE(estimator.averageValid());
}

void test_speed_changes_integrate_previous_speed_over_matching_counter_interval() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36);
    estimator.onSpeed(72, 50);
    estimator.onSpeed(0, 75);
    // Distance is not committed ahead of the next fuel observation.
    closeTo(0, estimator.totalDistanceM());
    estimator.onCounter(1, 100);
    closeTo(1, estimator.totalDistanceM()); // 0.05 * 10 + 0.025 * 20.
    closeTo(80, estimator.totalFuelMm3());
}

void test_reconfigure_resets_old_calibration_and_session() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36);
    drive(estimator, 36, 0, 100);
    auto config = enabledConfig();
    config.mm3PerTick = 103.7;
    estimator.configure(config);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    TEST_ASSERT_FALSE(estimator.averageValid());
    closeTo(0, estimator.totalFuelMm3());
    start(estimator, 36, 0, 20000);
    drive(estimator, 36, 20000, 100);
    TEST_ASSERT_EQUAL_UINT16(104, estimator.instantaneous());
    TEST_ASSERT_EQUAL_UINT16(104, estimator.average());
    closeTo(10370, estimator.totalFuelMm3());
}

void test_saturation_reserves_ffff_and_does_not_wrap_output() {
    FuelConsumptionEstimator estimator;
    auto config = enabledConfig();
    config.mm3PerTick = 100000;
    config.minAverageDistanceM = 1;
    estimator.configure(config);
    start(estimator, 36);
    drive(estimator, 36, 0, 10);
    TEST_ASSERT_TRUE(estimator.instantaneousValid());
    TEST_ASSERT_TRUE(estimator.averageValid());
    TEST_ASSERT_EQUAL_UINT16(65534, estimator.instantaneous());
    TEST_ASSERT_EQUAL_UINT16(65534, estimator.average());
}

void test_invalid_speed_and_numeric_overflow_invalidate_instead_of_publishing_nan() {
    FuelConsumptionEstimator estimator;
    estimator.configure(enabledConfig());
    start(estimator, 36);
    drive(estimator, 36, 0, 10);
    estimator.onSpeed(std::numeric_limits<double>::quiet_NaN(), 1100);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    closeTo(0, estimator.totalDistanceM());
    start(estimator, 36, 0, 2000);
    estimator.onSpeed(-1, 2100);
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    auto config = enabledConfig();
    config.mm3PerTick = std::numeric_limits<double>::max();
    estimator.configure(config);
    start(estimator, 36);
    estimator.onSpeed(36, 100);
    estimator.onCounter(2, 100); // Multiplication would overflow a double.
    TEST_ASSERT_FALSE(estimator.instantaneousValid());
    closeTo(0, estimator.totalFuelMm3());
    closeTo(0, estimator.totalDistanceM());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_config_rejects_nonfinite_invalid_ranges_and_ambiguous_full_wrap);
    RUN_TEST(test_disabled_and_invalid_configs_never_produce_measurements);
    RUN_TEST(test_known_constant_trip_converts_volume_and_distance_to_radio_units);
    RUN_TEST(test_fractional_speed_survives_distance_integration_and_rounding);
    RUN_TEST(test_counter_rollover_counts_only_modulo_difference);
    RUN_TEST(test_initial_counter_and_unsynchronized_start_do_not_invent_consumption);
    RUN_TEST(test_average_waits_for_distance_threshold);
    RUN_TEST(test_stopped_display_is_zero_but_idle_fuel_increases_session_average);
    RUN_TEST(test_starting_stationary_never_divides_by_zero_or_discards_idle_fuel);
    RUN_TEST(test_fuel_cut_while_moving_is_a_valid_zero);
    RUN_TEST(test_below_display_speed_keeps_consumed_volume);
    RUN_TEST(test_tick_invalidates_fuel_and_distance_when_entire_bus_falls_silent);
    RUN_TEST(test_speed_loss_cannot_mix_partial_fuel_with_stale_distance);
    RUN_TEST(test_counter_loss_cannot_keep_distance_from_a_previous_session);
    RUN_TEST(test_implausible_counter_jump_rebases_without_fabricating_volume);
    RUN_TEST(test_queued_frames_with_identical_timestamps_do_not_lose_increments);
    RUN_TEST(test_tick_budget_rejects_excessive_sustained_rate);
    RUN_TEST(test_millisecond_wrap_preserves_elapsed_time_and_timeout);
    RUN_TEST(test_speed_changes_integrate_previous_speed_over_matching_counter_interval);
    RUN_TEST(test_reconfigure_resets_old_calibration_and_session);
    RUN_TEST(test_saturation_reserves_ffff_and_does_not_wrap_output);
    RUN_TEST(test_invalid_speed_and_numeric_overflow_invalidate_instead_of_publishing_nan);
    return UNITY_END();
}
