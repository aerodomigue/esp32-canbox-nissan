/**
 * @file Config.h
 * @brief User-configurable parameters
 *
 * Modify these values to calibrate the system for your vehicle.
 * After changes, rebuild and flash: pio run --target upload
 */

#ifndef CONFIG_H
#define CONFIG_H

// =============================================================================
// STEERING ANGLE CALIBRATION
// =============================================================================

/**
 * Steering center offset
 * - Adjust if the camera guidelines show left/right when steering is straight
 * - Increase value if guidelines go right, decrease if they go left
 * - Typical range: -500 to +500
 */
#define STEER_CENTER_OFFSET 100

/**
 * Steering direction inversion
 * - Set to true if guidelines move opposite to steering direction
 * - Set to false for normal direction
 */
#define STEER_INVERT_DIRECTION true

/**
 * Steering scale factor (multiplier × 100)
 * - 100 = 1.0x (no scaling)
 * - 150 = 1.5x (more sensitive)
 * - 50 = 0.5x (less sensitive)
 * - Adjust if guidelines don't reach full range at steering lock
 */
#define STEER_SCALE_PERCENT 4

// =============================================================================
// INDICATOR TIMING
// =============================================================================

/**
 * Indicator timeout in milliseconds
 * - Time after last CAN signal before indicator is considered "off"
 * - Increase if indicators flicker on display
 * - Decrease if indicators stay on too long after turning off
 */
#define INDICATOR_TIMEOUT_MS 500

#endif // CONFIG_H
