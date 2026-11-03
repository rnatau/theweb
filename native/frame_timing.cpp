#include "frame_timing.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace web {
FrameMeasurement FrameTiming::presented(std::uint64_t timestamp_ns, std::uint64_t work_ns, unsigned steps, std::uint64_t game_frame) {
    if (anchored_ && timestamp_ns < previous_ns_)
        throw std::invalid_argument("Frame timestamp must be monotonic");
    const auto interval = anchored_ ? timestamp_ns - previous_ns_ : 0;
    if (interval) {
        intervals_[next_] = interval;
        next_ = (next_ + 1) % capacity;
        size_ = std::min(size_ + 1, capacity);
    }
    previous_ns_ = timestamp_ns;
    anchored_ = true;
    return {++frames_, game_frame, timestamp_ns, interval, work_ns, steps};
}
void FrameTiming::suspend() {
    anchored_ = false;
    size_ = 0;
    next_ = 0;
}
FrameStatistics FrameTiming::statistics() const {
    FrameStatistics result;
    result.samples = size_;
    if (!size_)
        return result;
    auto sorted = intervals_;
    std::sort(sorted.begin(), sorted.begin() + size_);
    double sum = 0;
    for (std::size_t i = 0; i < size_; i++) {
        sum += double(sorted[i]);
        result.over_25ms += sorted[i] > 25000000;
    }
    const auto mean = sum / double(size_);
    result.mean_ms = mean / 1000000;
    result.fps = 1000000000 / mean;
    double variance = 0;
    for (std::size_t i = 0; i < size_; i++) {
        const auto delta = double(sorted[i]) - mean;
        variance += delta * delta;
    }
    result.stddev_ms = std::sqrt(variance / double(size_)) / 1000000;
    result.p95_ms = double(sorted[(size_ * 95 + 99) / 100 - 1]) / 1000000;
    result.p99_ms = double(sorted[(size_ * 99 + 99) / 100 - 1]) / 1000000;
    result.max_ms = double(sorted[size_ - 1]) / 1000000;
    return result;
}
} // namespace web
