#include "M1990.hpp"
#include "PrusaGcodeSuite.hpp"

#include <Marlin.h>
#include <gcode/gcode.h>
#include <gcode/gcode_parser.hpp>
#include <module/motion.h>
#include <module/planner.h>
#include <printers.h>
#include <raii/scope_guard.hpp>

#include <algorithm>
#include <optional>

static_assert(PRINTER_IS_PRUSA_COREONEL());

namespace speed_test {

std::array<RunResult, max_runs> &results() {
    static std::array<RunResult, max_runs> instance;
    return instance;
}

Progress &progress() {
    static Progress instance;
    return instance;
}

} // namespace speed_test

namespace {

using speed_test::RunResult;
using Phase = speed_test::Progress::Phase;

/// Inset of the large pattern from the bed edges, so that a small skip does not slam the head into the frame
constexpr float bound = 20;
constexpr float small_pattern_size = 20;
constexpr float min_z = 30;
constexpr feedRate_t service_feedrate = 50;

/// Distance from the wall at which the probe starts; it travels twice as far before giving up
constexpr float probe_distance = 10;

/// Stallguard trigger noise stays well below this, while a step skip caused by an overloaded motor
/// loses at least a whole electrical cycle (4 full steps)
constexpr float skip_threshold_full_steps = 2;

struct Point {
    float x;
    float y;
};

/// Diagonals followed by a box, as in Klipper's TEST_SPEED macro
constexpr std::array<Point, 10> pattern(float x0, float y0, float x1, float y1) {
    return { { { x0, y0 }, { x1, y1 }, { x0, y0 }, { x1, y0 }, { x0, y1 }, { x1, y0 }, { x0, y0 }, { x0, y1 }, { x1, y1 }, { x1, y0 } } };
}

constexpr auto large_pattern = pattern(bound, bound, X_BED_SIZE - bound, Y_BED_SIZE - bound);
constexpr auto small_pattern = pattern(
    (X_BED_SIZE - small_pattern_size) / 2, (Y_BED_SIZE - small_pattern_size) / 2,
    (X_BED_SIZE + small_pattern_size) / 2, (Y_BED_SIZE + small_pattern_size) / 2);

/// Stalls the head against the X and Y homing walls without re-homing.
/// @returns trigger positions in the current coordinate frame, or nullopt if a wall was not reached
std::optional<xy_pos_t> probe_walls() {
    xy_pos_t triggered;
    LOOP_XY(i) {
        const AxisEnum axis = AxisEnum(i);
        const float dir = home_dir(axis);

        current_position[axis] = base_home_pos(axis) - dir * probe_distance;
        line_to_current_position(service_feedrate);
        planner.synchronize();

        if (!do_homing_move(axis, dir * 2 * probe_distance) || planner.draining()) {
            return std::nullopt;
        }
        triggered[axis] = current_position[axis];

        current_position[axis] -= dir * probe_distance;
        line_to_current_position(service_feedrate);
        planner.synchronize();
    }
    return triggered;
}

/// @returns false if the motion was aborted
bool run_patterns(feedRate_t speed, uint8_t iterations) {
    for (uint8_t i = 0; i < iterations; i++) {
        // Moves are queued ahead of the motion, so this leads the actual motion by up to a planner buffer
        speed_test::progress().iteration = i + 1;
        for (const auto *points : { &large_pattern, &small_pattern }) {
            for (const Point &point : *points) {
                current_position.x = point.x;
                current_position.y = point.y;
                line_to_current_position(speed);
                if (planner.draining()) {
                    return false;
                }
            }
        }
    }
    planner.synchronize();
    return !planner.draining();
}

RunResult::Status run(RunResult &result, float speed, float accel, uint8_t iterations) {
    auto &progress = speed_test::progress();

    progress.phase = Phase::homing;
    if (!GcodeSuite::G28_no_parser(true, true, true, { .only_if_needed = true })) {
        return RunResult::Status::failed;
    }
    do_blocking_move_to_z(std::max(current_position.z, min_z));

    progress.phase = Phase::measuring_before;
    const auto before = probe_walls();
    if (!before) {
        return RunResult::Status::failed;
    }

    const auto prev_settings = planner.user_settings;
    {
        ScopeGuard restore_settings = [&] { planner.apply_settings(prev_settings); };

        auto settings = prev_settings;
        LOOP_XY(axis) {
            settings.max_feedrate_mm_s[axis] = speed;
            settings.max_acceleration_mm_per_s2[axis] = static_cast<uint32_t>(accel);
        }
        settings.acceleration = accel;
        settings.travel_acceleration = accel;
        planner.apply_settings(settings);

        const auto &applied = planner.settings;
        result.speed = static_cast<uint16_t>(std::min(applied.max_feedrate_mm_s[X_AXIS], applied.max_feedrate_mm_s[Y_AXIS]));
        result.accel = std::min({ applied.max_acceleration_mm_per_s2[X_AXIS], applied.max_acceleration_mm_per_s2[Y_AXIS], static_cast<uint32_t>(applied.travel_acceleration) });

        SERIAL_ECHOLNPAIR("M1990: speed ", result.speed, " accel ", result.accel, " iterations ", iterations);

        progress.speed = result.speed;
        progress.accel = result.accel;
        progress.phase = Phase::moving;

        if (!run_patterns(result.speed, iterations)) {
            return RunResult::Status::failed;
        }
    }

    progress.phase = Phase::measuring_after;
    const auto after = probe_walls();

    // Leave the printer with a trustworthy position, whatever the outcome
    progress.phase = Phase::homing;
    if (!GcodeSuite::G28_no_parser(true, true, false)) {
        return RunResult::Status::failed;
    }

    if (!after) {
        SERIAL_ECHOLN("M1990: wall not found, position lost");
        return RunResult::Status::failed;
    }

    result.drift_x = after->x - before->x;
    result.drift_y = after->y - before->y;
    SERIAL_ECHOLNPAIR_F("M1990: drift X ", result.drift_x, 3);
    SERIAL_ECHOLNPAIR_F("M1990: drift Y ", result.drift_y, 3);

    const float threshold_x = skip_threshold_full_steps * X_MICROSTEPS / planner.settings.axis_steps_per_mm[X_AXIS];
    const float threshold_y = skip_threshold_full_steps * Y_MICROSTEPS / planner.settings.axis_steps_per_mm[Y_AXIS];
    if (std::abs(result.drift_x) > threshold_x || std::abs(result.drift_y) > threshold_y) {
        return RunResult::Status::skipped_steps;
    }
    return RunResult::Status::passed;
}

} // namespace

/** \addtogroup G-Codes
 * @{
 */

/**
 *### M1990: Speed test
 *
 * Moves the head along a large and a small diagonal-and-box pattern at the given speed and
 * acceleration, and checks for skipped steps by stalling the head against the X and Y homing
 * walls before and after the moves. Homes the printer if needed and re-homes X and Y at the end.
 *
 *#### Usage
 *
 *    M1990 [ S | A | I | R ]
 *
 *#### Parameters
 *
 * - `S` - Speed [mm/s], defaults to the current max XY feedrate
 * - `A` - Acceleration [mm/s^2], defaults to the current max XY acceleration
 * - `I` - Number of iterations of the pattern (1-3), defaults to 1
 * - `R` - Result slot (0-2) to store the result to, defaults to 0
 *
 *#### Examples
 *
 *    M1990 S300 A20000 I3 ; Test 300 mm/s at 20000 mm/s^2, three times
 */
void PrusaGcodeSuite::M1990() {
    GCodeParser2 p;
    if (!p.parse_marlin_command()) {
        return;
    }

    const uint8_t slot = std::min<uint8_t>(p.option<uint8_t>('R').value_or(0), speed_test::max_runs - 1);
    const float speed = p.option<float>('S').value_or(planner.user_settings.max_feedrate_mm_s[X_AXIS]);
    const float accel = p.option<float>('A').value_or(planner.user_settings.max_acceleration_mm_per_s2[X_AXIS]);
    const uint8_t iterations = std::clamp<uint8_t>(p.option<uint8_t>('I').value_or(1), 1, speed_test::max_iterations);

    auto &progress = speed_test::progress();
    progress.slot = slot;
    progress.iteration = 0;
    progress.iterations = iterations;
    progress.speed = static_cast<uint16_t>(speed);
    progress.accel = static_cast<uint32_t>(accel);

    auto &result = speed_test::results()[slot];
    result = {};
    result.status = run(result, speed, accel, iterations);

    progress.phase = Phase::idle;
}

/** @}*/
