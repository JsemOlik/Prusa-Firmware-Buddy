#pragma once

#include <array>
#include <cstdint>

namespace speed_test {

inline constexpr uint8_t max_iterations = 3;

/// Number of speed/acceleration combinations that can be tested in one go
inline constexpr uint8_t max_runs = 3;

struct RunResult {
    enum class Status : uint8_t {
        not_run,
        passed,
        skipped_steps,
        /// Homing or the wall probe failed, or the test was aborted
        failed,
    };

    Status status = Status::not_run;

    /// Speed actually used, after the planner limits were applied [mm/s]
    uint16_t speed = 0;

    /// Acceleration actually used, after the planner limits were applied [mm/s^2]
    uint32_t accel = 0;

    /// Position drift measured against the homing walls [mm]
    float drift_x = 0;
    float drift_y = 0;
};

/// Results of the last M1990 runs, indexed by the R parameter.
/// Written by the marlin thread; only read them once M1990 has finished.
std::array<RunResult, max_runs> &results();

} // namespace speed_test
