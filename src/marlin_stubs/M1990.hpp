#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <speed_test_result.hpp>

namespace speed_test {

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

    /// Set by the UI to stop M1990 as soon as possible. Not cleared by M1990, so that it also
    /// skips any further queued M1990s.
    std::atomic<bool> cancel = false;
};

Progress &progress();

/// Results of the last M1990 runs, indexed by the R parameter.
/// Written by the marlin thread; only read them once M1990 has finished.
RunResults &results();

/// Result of the last M1990 acceleration search (the F parameter).
/// Written by the marlin thread; only read it once M1990 has finished.
SearchResult &search_result();

} // namespace speed_test
