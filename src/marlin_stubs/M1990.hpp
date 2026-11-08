#pragma once

#include <array>
#include <atomic>
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

/// State of the running M1990, for display purposes.
/// The fields are updated independently, so they may be briefly inconsistent with each other.
struct Progress {
    enum class Phase : uint8_t {
        idle,
        homing,
        measuring_before,
        moving,
        measuring_after,
    };

    std::atomic<Phase> phase = Phase::idle;
    std::atomic<uint8_t> slot = 0;

    /// 1-based, valid in Phase::moving
    std::atomic<uint8_t> iteration = 0;
    std::atomic<uint8_t> iterations = 0;

    /// [mm/s]
    std::atomic<uint16_t> speed = 0;

    /// [mm/s^2]
    std::atomic<uint32_t> accel = 0;
};

Progress &progress();

/// Results of the last M1990 runs, indexed by the R parameter.
/// Written by the marlin thread; only read them once M1990 has finished.
std::array<RunResult, max_runs> &results();

} // namespace speed_test
