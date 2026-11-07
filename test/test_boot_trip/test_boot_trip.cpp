#include <unity.h>
#include "BootTripComputer.h"

#include <cmath>
#include <limits>

void setUp() {}
void tearDown() {}

namespace {
void closeTo(double expected, double actual, double tolerance = 1e-7) {
    TEST_ASSERT_TRUE_MESSAGE(std::fabs(expected - actual) <= tolerance,
                             "Unexpected trip distance");
}

void drive(BootTripComputer& trip, double speed, uint32_t startAt,
           unsigned samples, uint32_t period = 100) {
    for (unsigned i = 1; i <= samples; ++i) {
        trip.onSpeed(speed, startAt + i * period);
    }
}
}

void test_empty_trip_has_no_values_and_setup_time_is_part_of_boot() {
    BootTripComputer trip;
    TEST_ASSERT_FALSE(trip.hasStarted());
    TEST_ASSERT_FALSE(trip.valid());
    TEST_ASSERT_TRUE(trip.complete());
    TEST_ASSERT_EQUAL_UINT32(0, trip.elapsedSeconds());
    TEST_ASSERT_EQUAL_UINT16(0, trip.averageSpeedDeciKmh());
    trip.tick(5000);
    TEST_ASSERT_EQUAL_UINT32(5, trip.elapsedSeconds());
    trip.onSpeed(36, 5000);
    drive(trip, 36, 5000, 100);
    closeTo(100, trip.distanceM());
    TEST_ASSERT_TRUE(trip.valid());
    TEST_ASSERT_EQUAL_UINT32(15, trip.elapsedSeconds());
    TEST_ASSERT_EQUAL_UINT16(240, trip.averageSpeedDeciKmh());
}

void test_stationary_boot_is_a_valid_zero_and_idle_counts_in_mean_speed() {
    BootTripComputer trip;
    trip.onSpeed(0, 0);
    drive(trip, 0, 0, 50);
    TEST_ASSERT_TRUE(trip.valid());
    TEST_ASSERT_TRUE(trip.hasStarted());
    closeTo(0, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT16(0, trip.averageSpeedDeciKmh());
    trip.onSpeed(36, 5000);
    drive(trip, 36, 5000, 100);
    trip.onSpeed(0, 15000);
    drive(trip, 0, 15000, 50);
    closeTo(100, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT32(20, trip.elapsedSeconds());
    TEST_ASSERT_EQUAL_UINT16(180, trip.averageSpeedDeciKmh());
}

void test_previous_speed_is_held_and_ticks_do_not_double_integrate() {
    BootTripComputer trip;
    trip.onSpeed(36, 0);
    trip.tick(100);
    trip.tick(100); // Publishing twice at one timestamp must be idempotent.
    trip.onSpeed(72, 100);
    trip.tick(200);
    trip.onSpeed(0, 200);
    trip.tick(300);
    closeTo(3, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT16(360, trip.averageSpeedDeciKmh());
    TEST_ASSERT_TRUE(trip.valid());
}

void test_mean_is_distance_over_full_boot_time_not_mean_of_speed_samples() {
    BootTripComputer trip;
    trip.onSpeed(36, 0);
    drive(trip, 36, 0, 10);
    trip.onSpeed(72, 1000);
    drive(trip, 72, 1000, 20);
    closeTo(50, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT32(3, trip.elapsedSeconds());
    TEST_ASSERT_EQUAL_UINT16(600, trip.averageSpeedDeciKmh());
}

void test_exact_freshness_boundary_is_valid_but_later_gap_is_not_invented() {
    BootTripComputer trip;
    trip.onSpeed(36, 0);
    trip.tick(500);
    closeTo(5, trip.distanceM());
    TEST_ASSERT_TRUE(trip.valid());
    trip.tick(501);
    closeTo(5, trip.distanceM()); // Do not add a stale millisecond.
    TEST_ASSERT_FALSE(trip.complete());
    TEST_ASSERT_FALSE(trip.valid());
    trip.tick(10000);
    closeTo(5, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT32(10, trip.elapsedSeconds());
}

void test_gap_larger_than_boundary_keeps_only_previously_integrated_distance() {
    BootTripComputer trip;
    trip.onSpeed(36, 0);
    trip.tick(200);
    trip.tick(1000);
    closeTo(2, trip.distanceM());
    TEST_ASSERT_FALSE(trip.valid());
    trip.onSpeed(72, 2000); // Fresh baseline, not a bridge over the missing data.
    drive(trip, 72, 2000, 10);
    closeTo(22, trip.distanceM());
    TEST_ASSERT_FALSE(trip.complete());
    TEST_ASSERT_FALSE(trip.valid());
    TEST_ASSERT_EQUAL_UINT32(3, trip.elapsedSeconds());
}

void test_new_object_is_the_only_reset_of_boot_totals_and_completeness() {
    BootTripComputer trip;
    trip.onSpeed(36, 0);
    drive(trip, 36, 0, 100);
    trip.markDiscontinuity();
    trip.markDiscontinuity();
    closeTo(100, trip.distanceM());
    TEST_ASSERT_FALSE(trip.valid());
    trip.onSpeed(36, 10000);
    drive(trip, 36, 10000, 100);
    closeTo(200, trip.distanceM());
    TEST_ASSERT_FALSE(trip.complete());
    BootTripComputer nextBoot;
    TEST_ASSERT_TRUE(nextBoot.complete());
    TEST_ASSERT_FALSE(nextBoot.hasStarted());
    closeTo(0, nextBoot.distanceM());
    TEST_ASSERT_EQUAL_UINT32(0, nextBoot.elapsedSeconds());
    nextBoot.onSpeed(0, 0);
    TEST_ASSERT_TRUE(nextBoot.valid());
}

void test_reusing_same_object_does_not_reset_trip_or_pending_interval() {
    BootTripComputer trip;
    trip.onSpeed(36, 0);
    drive(trip, 36, 0, 50);
    trip.tick(5050);
    // An unchanged profile reload keeps this exact instance and its sources.
    BootTripComputer& afterReload = trip;
    afterReload.onSpeed(72, 5100);
    drive(afterReload, 72, 5100, 50);
    closeTo(151, afterReload.distanceM());
    TEST_ASSERT_EQUAL_UINT32(10, afterReload.elapsedSeconds());
    TEST_ASSERT_EQUAL_UINT16(538, afterReload.averageSpeedDeciKmh());
    TEST_ASSERT_TRUE(afterReload.valid());
}

void test_invalid_speed_marks_started_trip_incomplete_without_poisoning_numbers() {
    const double invalid[] = {
        -0.01, 300.0, 999.0,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (double speed : invalid) {
        BootTripComputer trip;
        trip.onSpeed(36, 0);
        trip.tick(100);
        trip.onSpeed(speed, 100);
        closeTo(1, trip.distanceM());
        TEST_ASSERT_FALSE(trip.complete());
        TEST_ASSERT_FALSE(trip.valid());
        trip.onSpeed(36, 200);
        drive(trip, 36, 200, 10);
        closeTo(11, trip.distanceM());
        TEST_ASSERT_FALSE(trip.valid());
    }
}

void test_invalid_data_before_first_speed_has_no_trip_to_invalidate() {
    BootTripComputer trip;
    trip.markDiscontinuity();
    trip.onSpeed(300, 1000);
    TEST_ASSERT_FALSE(trip.hasStarted());
    TEST_ASSERT_FALSE(trip.valid());
    TEST_ASSERT_TRUE(trip.complete());
    trip.onSpeed(299.99, 1000);
    drive(trip, 299.99, 1000, 10);
    TEST_ASSERT_TRUE(trip.valid());
    TEST_ASSERT_EQUAL_UINT16(1500, trip.averageSpeedDeciKmh());
}

void test_distance_increments_floor_at_completed_100_m_and_mean_rounds_tenths() {
    BootTripComputer trip;
    trip.onSpeed(36, 0);
    drive(trip, 36, 0, 99);
    closeTo(99, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT32(0, trip.distanceDeciKm());
    trip.tick(9999);
    closeTo(99.99, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT32(0, trip.distanceDeciKm());
    trip.onSpeed(36, 10000);
    closeTo(100, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT32(1, trip.distanceDeciKm());
    TEST_ASSERT_EQUAL_UINT16(360, trip.averageSpeedDeciKmh());
    trip.onSpeed(0, 10000);
    trip.tick(10100);
    TEST_ASSERT_EQUAL_UINT16(356, trip.averageSpeedDeciKmh());
}

void test_subsecond_uptime_retains_precision_and_does_not_divide_by_zero() {
    BootTripComputer trip;
    trip.onSpeed(36.5, 0);
    TEST_ASSERT_EQUAL_UINT16(0, trip.averageSpeedDeciKmh());
    drive(trip, 36.5, 0, 5);
    TEST_ASSERT_EQUAL_UINT32(0, trip.elapsedSeconds());
    TEST_ASSERT_EQUAL_UINT16(365, trip.averageSpeedDeciKmh());
    closeTo(36.5 / 7.2, trip.distanceM());
}

void test_uptime_and_speed_tracking_survive_millisecond_rollover() {
    BootTripComputer trip;
    const uint32_t origin = std::numeric_limits<uint32_t>::max() - 550;
    trip.tick(origin);
    trip.onSpeed(36, origin);
    drive(trip, 36, origin, 20);
    closeTo(20, trip.distanceM());
    const uint64_t totalMs = static_cast<uint64_t>(origin) + 2000;
    TEST_ASSERT_EQUAL_UINT32(totalMs / 1000, trip.elapsedSeconds());
    TEST_ASSERT_TRUE(trip.valid());
    trip.tick(origin + 2501u);
    TEST_ASSERT_FALSE(trip.valid());
    closeTo(20, trip.distanceM());
}

void test_uptime_remains_total_after_24_hours() {
    BootTripComputer trip;
    trip.tick(24u * 60u * 60u * 1000u);
    TEST_ASSERT_EQUAL_UINT32(86400, trip.elapsedSeconds());
    trip.onSpeed(36, 86400000);
    drive(trip, 36, 86400000, 100);
    closeTo(100, trip.distanceM());
    TEST_ASSERT_EQUAL_UINT32(86410, trip.elapsedSeconds());
    TEST_ASSERT_EQUAL_UINT16(0, trip.averageSpeedDeciKmh());
    TEST_ASSERT_TRUE(trip.valid());
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_empty_trip_has_no_values_and_setup_time_is_part_of_boot);
    RUN_TEST(test_stationary_boot_is_a_valid_zero_and_idle_counts_in_mean_speed);
    RUN_TEST(test_previous_speed_is_held_and_ticks_do_not_double_integrate);
    RUN_TEST(test_mean_is_distance_over_full_boot_time_not_mean_of_speed_samples);
    RUN_TEST(test_exact_freshness_boundary_is_valid_but_later_gap_is_not_invented);
    RUN_TEST(test_gap_larger_than_boundary_keeps_only_previously_integrated_distance);
    RUN_TEST(test_new_object_is_the_only_reset_of_boot_totals_and_completeness);
    RUN_TEST(test_reusing_same_object_does_not_reset_trip_or_pending_interval);
    RUN_TEST(test_invalid_speed_marks_started_trip_incomplete_without_poisoning_numbers);
    RUN_TEST(test_invalid_data_before_first_speed_has_no_trip_to_invalidate);
    RUN_TEST(test_distance_increments_floor_at_completed_100_m_and_mean_rounds_tenths);
    RUN_TEST(test_subsecond_uptime_retains_precision_and_does_not_divide_by_zero);
    RUN_TEST(test_uptime_and_speed_tracking_survive_millisecond_rollover);
    RUN_TEST(test_uptime_remains_total_after_24_hours);
    return UNITY_END();
}
