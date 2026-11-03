#pragma once
#include "../physics/ball.hpp"
#include <stdexcept>

namespace web {
// Original 0x402ac4 drawing gates. Dirty-region history is outside this policy.
class BallPresentationPolicy {
public:
    struct Draws {
        bool ball = false;
        bool shadow = false;
    };
    explicit BallPresentationPolicy(unsigned detail = 3) :
        detail_(detail) {
        if (detail < 1 || detail > 3)
            throw std::out_of_range("Unknown original graphic detail");
    }
    bool low_detail() const {
        return detail_ == 1;
    }
    Draws evaluate(const physics::BallState& ball, std::int32_t live_balls) const {
        const auto type = ball.motion_type();
        const bool visible = ball.active() != 0 && (type == physics::ball_type::free || type == physics::ball_type::launcher || type == physics::ball_type::legacy_mobile);
        return {visible, visible && ball.motion_mode() == physics::motion_mode::table && ball.layer() == 0 && (live_balls == 1 || detail_ == 3)};
    }

private:
    unsigned detail_;
};
} // namespace web
