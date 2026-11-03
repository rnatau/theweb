#pragma once
#include "../input.hpp"
#include <cstdint>
#include <functional>
#include <span>
#include <vector>

namespace web {
// Original 0x401080. The host supplies the synchronous tilt callback (0x412758).
// Field order matches the addresses in tools/verify-native.py.
struct NudgeState {
    std::int32_t manual_x = 0;
    std::uint32_t decay_tick = 0;
    std::int32_t active = 0, tilt = 0, positive_held = 0, y = 0, x = 0, negative_held = 0, up_held = 0, vy = 0, vx = 0, axis_stop = 0;
};
// Optional synchronous boundary at the original tilt call sites, before velocity
// integration continues. The return value remains the total request count.
unsigned update_nudge(NudgeState& state, std::uint16_t actions, const std::function<void()>& on_tilt = {});
std::vector<std::uint8_t> nudge_trace(std::span<const std::uint8_t> actions);

namespace physics {
class NudgeController {
public:
    unsigned step(std::uint16_t actions) {
        return update_nudge(state_, actions);
    }
    void reset() {
        state_ = {};
    }
    const NudgeState& state() const {
        return state_;
    }

private:
    NudgeState state_{};
};
} // namespace physics
} // namespace web
