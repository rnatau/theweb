#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace web {
struct FrameStatistics {
    std::size_t samples = 0;
    double fps = 0, mean_ms = 0, p95_ms = 0, p99_ms = 0, max_ms = 0, stddev_ms = 0;
    std::size_t over_25ms = 0;
};
struct FrameMeasurement {
    std::uint64_t frame = 0, game_frame = 0, timestamp_ns = 0, interval_ns = 0, work_ns = 0;
    unsigned simulation_steps = 0;
};
// Presentation diagnostics only: neither the clock nor physics reads these data.
// Pause/focus/view/resize boundaries break the interval so intentional inactivity
// cannot appear as a long game frame. Actual slow active frames remain included.
class FrameTiming {
public:
    static constexpr std::size_t capacity = 240;
    FrameMeasurement presented(std::uint64_t timestamp_ns, std::uint64_t work_ns, unsigned steps, std::uint64_t game_frame);
    void suspend();
    FrameStatistics statistics() const;
    std::uint64_t frames() const {
        return frames_;
    }

private:
    std::array<std::uint64_t, capacity> intervals_{};
    std::size_t size_ = 0, next_ = 0;
    std::uint64_t frames_ = 0, previous_ns_ = 0;
    bool anchored_ = false;
};
} // namespace web
