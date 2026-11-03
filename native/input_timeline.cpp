#include "input_timeline.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace web {
std::uint64_t input_event_time(std::uint64_t receipt_ns, std::uint32_t now_ms, std::uint32_t event_ms) {
    const auto age_ms = std::uint32_t(now_ms - event_ms);
    if (age_ms > std::uint32_t(std::numeric_limits<std::int32_t>::max()))
        return receipt_ns;
    const auto age_ns = std::uint64_t(age_ms) * 1000000;
    return receipt_ns > age_ns ? receipt_ns - age_ns : 0;
}
void InputTimeline::reset(std::uint64_t timestamp_ns, std::uint32_t keyboard, std::uint32_t controller) {
    events_.clear();
    states_ = latest_ = {keyboard, controller};
    latest_time_.fill(timestamp_ns);
    consumed_ns_ = timestamp_ns;
}
void InputTimeline::record(TimestampedInput event) {
    const auto source = std::size_t(event.source);
    if (source >= states_.size())
        throw std::invalid_argument("Unknown input source");
    if (event.actions == latest_[source])
        return;
    // Late delivery cannot rewrite completed physics. Preserve per-device order
    // while allowing differently delayed devices to merge chronologically.
    event.timestamp_ns = std::max({event.timestamp_ns, consumed_ns_, latest_time_[source]});
    latest_[source] = event.actions;
    latest_time_[source] = event.timestamp_ns;
    const auto position = std::upper_bound(events_.begin(), events_.end(), event.timestamp_ns, [](auto time, const auto& queued) { return time < queued.timestamp_ns; });
    events_.insert(position, event);
}
TimedInputFrame InputTimeline::frame(std::uint64_t start_ns, std::uint64_t end_ns, unsigned microsteps) {
    if (start_ns < consumed_ns_ || end_ns <= start_ns || !microsteps || microsteps > 2100)
        throw std::invalid_argument("Invalid timed input frame");
    TimedInputFrame result;
    result.microsteps.resize(microsteps);
    std::size_t next = 0;
    const auto apply = [&](const TimestampedInput& event) {
        const auto before = states_[0] | states_[1];
        states_[std::size_t(event.source)] = event.actions;
        result.released |= before & ~(states_[0] | states_[1]);
        return (states_[0] | states_[1]) & ~before;
    };
    // A discarded wall-clock gap carries only the final held state forward.
    while (next < events_.size() && events_[next].timestamp_ns < start_ns)
        apply(events_[next++]);
    result.released = 0;
    for (unsigned tick = 0; tick < microsteps; ++tick) {
        const auto boundary = start_ns + (end_ns - start_ns) * (tick + 1) / microsteps;
        auto& sample = result.microsteps[tick];
        while (next < events_.size() && events_[next].timestamp_ns < boundary)
            sample.pressed |= apply(events_[next++]);
        sample.actions = states_[0] | states_[1];
        result.rule_actions |= sample.pressed;
        result.pressed |= sample.pressed;
    }
    result.rule_actions |= states_[0] | states_[1];
    events_.erase(events_.begin(), events_.begin() + next);
    consumed_ns_ = end_ns;
    return result;
}
InputTransitions InputTimeline::sample(std::uint64_t start_ns, std::uint64_t end_ns) {
    start_ns = std::max(start_ns, consumed_ns_);
    if (end_ns < start_ns)
        throw std::invalid_argument("Incremental input moved backwards");
    InputTransitions result;
    std::size_t next = 0;
    while (next < events_.size() && events_[next].timestamp_ns < end_ns) {
        const auto& event = events_[next++];
        const auto before = states_[0] | states_[1];
        states_[std::size_t(event.source)] = event.actions;
        if (event.timestamp_ns >= start_ns) {
            result.pressed |= (states_[0] | states_[1]) & ~before;
            result.released |= before & ~(states_[0] | states_[1]);
        }
    }
    result.actions = states_[0] | states_[1];
    events_.erase(events_.begin(), events_.begin() + next);
    consumed_ns_ = end_ns;
    return result;
}
namespace {
void append(std::vector<std::uint8_t>& bytes, std::uint64_t value, unsigned count) {
    for (unsigned i = 0; i < count; ++i)
        bytes.push_back(std::uint8_t(value >> (i * 8)));
}
std::uint64_t word(const std::vector<std::uint8_t>& bytes, std::size_t offset, unsigned count) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < count; ++i)
        value |= std::uint64_t(bytes.at(offset + i)) << (i * 8);
    return value;
}
} // namespace
std::vector<std::uint8_t> serialize_timed_input(const TimedInputReplay& replay) {
    if ((replay.version != 1 && replay.version != 2) || replay.events.size() > std::numeric_limits<std::uint32_t>::max())
        throw std::invalid_argument("Too many input replay events");
    std::vector<std::uint8_t> bytes{'T', 'W', 'I', std::uint8_t('0' + replay.version)};
    append(bytes, 60, 4);
    append(bytes, replay.frames, 4);
    append(bytes, replay.events.size(), 4);
    std::uint64_t previous = 0;
    for (const auto& event : replay.events) {
        if (std::uint32_t(event.source) > 1 || event.timestamp_ns < previous || event.timestamp_ns >= (std::uint64_t(replay.frames) * 1000000000 + 59) / 60)
            throw std::invalid_argument("Input replay event is outside its timeline");
        previous = event.timestamp_ns;
        append(bytes, event.timestamp_ns, 8);
        append(bytes, std::uint32_t(event.source), 4);
        append(bytes, event.actions, 4);
    }
    return bytes;
}
TimedInputReplay parse_timed_input(const std::vector<std::uint8_t>& bytes) {
    if (bytes.size() < 16 || bytes[0] != 'T' || bytes[1] != 'W' || bytes[2] != 'I' || (bytes[3] != '1' && bytes[3] != '2') || word(bytes, 4, 4) != 60 ||
        word(bytes, 12, 4) != (bytes.size() - 16) / 16 || (bytes.size() - 16) % 16)
        throw std::invalid_argument("Unsupported or truncated timestamped input replay");
    TimedInputReplay replay;
    replay.version = unsigned(bytes[3] - '0');
    replay.frames = std::uint32_t(word(bytes, 8, 4));
    for (std::size_t offset = 16; offset < bytes.size(); offset += 16)
        replay.events.push_back({word(bytes, offset, 8), InputSource(word(bytes, offset + 8, 4)), std::uint32_t(word(bytes, offset + 12, 4))});
    serialize_timed_input(replay); // Shared bounds/source validation.
    return replay;
}
} // namespace web
