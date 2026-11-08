#pragma once

#include <array>
#include <cstdint>

namespace speed_test {

inline constexpr uint8_t max_iterations = 3;

/// Number of speed/acceleration combinations that can be tested in one go
inline constexpr uint8_t max_runs = 3;

struct __attribute__((packed)) RunResult {
    // DO NOT CHANGE LAYOUT OF THIS STRUCT, it is persisted in the config store

    /// New statuses go to the end
    enum class Status : uint8_t {
        not_run,
        passed,
        skipped_steps,
        /// Homing or the wall probe failed, or the motion was aborted
        failed,
        cancelled,
    };

    Status status = Status::not_run;

    /// Speed actually used, after the planner limits were applied [mm/s]
    uint16_t speed = 0;

    /// Acceleration actually used, after the planner limits were applied [mm/s^2]
    uint32_t accel = 0;

    /// Position drift measured against the homing walls [mm]
    float drift_x = 0;
    float drift_y = 0;

    bool operator==(const RunResult &) const = default;
};

using RunResults = std::array<RunResult, max_runs>;

/// Outcome of a search for the highest acceleration that does not skip steps
struct __attribute__((packed)) SearchResult {
    // DO NOT CHANGE LAYOUT OF THIS STRUCT, it is persisted in the config store

    /// Outcome of the last test of the search: passed if the hardware limit was reached
    RunResult::Status status = RunResult::Status::not_run;

    /// [mm/s], 0 if no search was run
    uint16_t speed = 0;

    /// Highest acceleration that passed, 0 if none did [mm/s^2]
    uint32_t passed_accel = 0;

    /// Acceleration that skipped steps, 0 if none did [mm/s^2]
    uint32_t failed_accel = 0;

    /// Max acceleration with a safety margin from the limit found [mm/s^2], 0 if the search did not finish
    constexpr uint32_t recommended_accel() const {
        constexpr uint32_t round_to = 100;
        switch (status) {
        case RunResult::Status::skipped_steps:
            return failed_accel * 4 / 5 / round_to * round_to;
        case RunResult::Status::passed:
            return passed_accel;
        default:
            return 0;
        }
    }

    bool operator==(const SearchResult &) const = default;
};

} // namespace speed_test
