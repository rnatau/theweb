#pragma once
#include "../../render/nudge_feedback.hpp"
#include "../../input.hpp"

namespace web {
// User-requested presentation enhancement. Retains the original impulse size,
// but shows the cabinet push and settles over 200 ms of the 60 Hz clock.
// It never writes simulation state or advances during repainting.
class NudgeAnimation {
public:
    void trigger(std::uint32_t actions) {
        const bool left = actions & input_action::nudge_negative;
        const bool right = actions & input_action::nudge_positive;
        // Side nudges push the cabinet inward from the named side.
        direction_ = {int(left) - int(right), (actions & input_action::nudge_up) || (left && right) ? -1 : 0};
        remaining_ = direction_ == NudgeOffset{} ? 0 : duration;
    }
    void advance(unsigned ticks) {
        remaining_ -= std::min(remaining_, ticks);
    }
    NudgeOffset offset(unsigned variant) const {
        const unsigned distance = 2 * (1 + ((variant & 0x30) >> 4));
        const int pixels = int((distance * remaining_ * remaining_ + duration * duration / 2) / (duration * duration));
        return {direction_.x * pixels, direction_.y * pixels};
    }

private:
    static constexpr unsigned duration = 12;
    unsigned remaining_ = 0;
    NudgeOffset direction_;
};
} // namespace web
