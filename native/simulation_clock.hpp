#pragma once
#include <cstdint>

namespace web {
// Integer-nanosecond 60 Hz scheduler, independent of paint timing. Excess elapsed
// time is discarded on focus loss/pause by reset(), never integrated as a jump.
class SimulationClock {
public:
    static constexpr std::uint64_t frames_per_second = 60, nanoseconds_per_second = 1000000000;
    unsigned advance(std::uint64_t nanoseconds);
    std::uint64_t until_next_frame_ns() const {
        return (nanoseconds_per_second - remainder_ + frames_per_second - 1) / frames_per_second;
    }
    // Ceiling of a completed tick boundary in the caller's monotonic clock.
    std::uint64_t frame_boundary_ns(std::uint64_t now_ns, unsigned frames_ago = 0) const {
        return now_ns - (remainder_ + std::uint64_t(frames_ago) * nanoseconds_per_second) / frames_per_second;
    }
    void reset() {
        remainder_ = 0;
    }

private:
    std::uint64_t remainder_ = 0;
};
} // namespace web
