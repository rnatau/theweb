#pragma once
#include "input.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace web {
// A reported state transition, rather than an inferred hardware sample.
enum class InputSource : std::uint32_t {
    keyboard,
    controller
};
struct TimestampedInput {
    std::uint64_t timestamp_ns = 0;
    InputSource source = InputSource::keyboard;
    std::uint32_t actions = 0;
};
struct TimedInputSample {
    std::uint32_t actions = 0, pressed = 0;
};
struct InputTransitions {
    std::uint32_t actions = 0, pressed = 0, released = 0;
};
struct TimedInputFrame {
    std::uint32_t rule_actions = 0, pressed = 0, released = 0;
    std::vector<TimedInputSample> microsteps;
};
// Convert a wrapping platform millisecond event clock to the host monotonic
// clock. Future/invalid platform times use receipt time; stale times remain old.
std::uint64_t input_event_time(std::uint64_t receipt_ns, std::uint32_t now_ms, std::uint32_t event_ms);
class InputTimeline {
public:
    void reset(std::uint64_t timestamp_ns, std::uint32_t keyboard = 0, std::uint32_t controller = 0);
    void record(TimestampedInput);
    // Half-open interval [start,end). Each transition enters the microstep bin
    // containing its timestamp. Rules retain a once-per-tick press latch.
    TimedInputFrame frame(std::uint64_t start_ns, std::uint64_t end_ns, unsigned microsteps);
    // Causal consumption for a single completed microstep. A discarded gap
    // carries held state forward without replaying obsolete command taps.
    InputTransitions sample(std::uint64_t start_ns, std::uint64_t end_ns);

private:
    std::vector<TimestampedInput> events_;
    std::array<std::uint32_t, 2> states_{}, latest_{};
    std::array<std::uint64_t, 2> latest_time_{};
    std::uint64_t consumed_ns_ = 0;
};
struct TimedInputReplay {
    std::uint32_t frames = 0;
    std::vector<TimestampedInput> events;
    unsigned version = 1; // TWI1 historical batches; TWI2 causal incremental input.
};
// TWI1: magic, 60 Hz, frame count, event count; then little-endian
// (uint64 nanoseconds, uint32 source, uint32 action state) records.
std::vector<std::uint8_t> serialize_timed_input(const TimedInputReplay&);
TimedInputReplay parse_timed_input(const std::vector<std::uint8_t>&);
} // namespace web
