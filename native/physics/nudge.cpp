#include "nudge.hpp"
#include "../web.hpp"
#include "integer.hpp"
#include "units.hpp"
#include <algorithm>
#include <stdexcept>

namespace web {
static void put32(Bytes& b, std::size_t o, std::uint32_t v) {
    for (unsigned i = 0; i < 4; i++)
        b.at(o + i) = std::uint8_t(v >> (8 * i));
}
namespace {
// Nudge integration runs on physics microsteps, not on the 60 Hz rules clock.
constexpr int acceleration = 0xc0, recenter_velocity = 0xccc;
constexpr int displacement_limit = 3 * physics::fixed_point::position_one;
constexpr int tilt_threshold = 0x100;
constexpr unsigned decay_period_mask = 0x3f; // Decay once every 64 inactive microsteps.
} // namespace
unsigned update_nudge(NudgeState& s, std::uint16_t actions, const std::function<void()>& on_tilt) {
    using physics::integer::add;
    using physics::integer::sub;
    unsigned tilt_requests = 0;
    const auto request = [&] {
        ++tilt_requests;
        if (on_tilt)
            on_tilt();
    };
    s.axis_stop = 0;
    s.manual_x = 0;
    s.active = 0;
    s.up_held = (actions & input_action::nudge_up) != 0;
    s.positive_held = (actions & input_action::nudge_positive) != 0;
    s.negative_held = (actions & input_action::nudge_negative) != 0;
    if (s.positive_held && s.negative_held) {
        s.positive_held = 0;
        s.negative_held = 0;
        s.up_held = 1;
    }
    if (!s.up_held)
        s.vy = -recenter_velocity;
    else {
        s.tilt = add(s.tilt, 1);
        s.active = 1;
        if (s.tilt == tilt_threshold)
            request();
        s.vy = add(s.vy, acceleration);
    }
    s.y = add(s.y, s.vy);
    if (s.y >= displacement_limit) {
        s.y = displacement_limit;
        s.vy = 0;
    } else if (s.y < 1) {
        s.y = 0;
        s.vy = 0;
    }
    if (s.positive_held) {
        s.active = 1;
        s.tilt = add(s.tilt, 1);
        if (s.tilt == tilt_threshold)
            request();
        s.vx = add(s.vx, acceleration);
    } else if (s.negative_held) {
        s.active = 1;
        s.tilt = add(s.tilt, 1);
        if (s.tilt == tilt_threshold)
            request();
        s.vx = sub(s.vx, acceleration);
    } else {
        s.vx = s.x < 1 ? recenter_velocity : -recenter_velocity;
        s.x = add(s.x, s.vx);
        s.manual_x = 1;
        if ((s.vx > 0 && s.x >= 0) || (s.vx < 0 && s.x < 1)) {
            s.x = 0;
            s.vx = 0;
        }
    }
    if (!s.manual_x) {
        s.x = add(s.x, s.vx);
        if (s.x > displacement_limit) {
            s.x = displacement_limit;
            s.vx = 0;
        } else if (s.x < -displacement_limit) {
            s.x = -displacement_limit;
            s.vx = 0;
        }
    }
    if (s.vx == 0 || s.vy == 0)
        s.axis_stop = 1;
    s.tilt = std::min(s.tilt, tilt_threshold);
    s.decay_tick++;
    if (!s.active && (s.decay_tick & decay_period_mask) == 0)
        s.tilt = std::max(0, sub(s.tilt, 1));
    return tilt_requests;
}
Bytes nudge_trace(std::span<const std::uint8_t> actions) {
    if (actions.size() % 2)
        throw std::runtime_error("Truncated action trace");
    physics::NudgeController controller;
    Bytes output(actions.size() / 2 * 52);
    for (std::size_t o = 0; o < actions.size(); o += 2) {
        auto requests = controller.step(u16(actions, o));
        const auto& state = controller.state();
        std::array<std::uint32_t, 13> fields = {
            std::uint32_t(state.manual_x),
            state.decay_tick,
            std::uint32_t(state.active),
            std::uint32_t(state.tilt),
            std::uint32_t(state.positive_held),
            std::uint32_t(state.y),
            std::uint32_t(state.x),
            std::uint32_t(state.negative_held),
            std::uint32_t(state.up_held),
            std::uint32_t(state.vy),
            std::uint32_t(state.vx),
            std::uint32_t(state.axis_stop),
            requests};
        for (std::size_t i = 0; i < fields.size(); i++)
            put32(output, o / 2 * 52 + i * 4, fields[i]);
    }
    return output;
}
} // namespace web
