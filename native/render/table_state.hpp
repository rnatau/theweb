#pragma once
#include "../physics/simulation.hpp"
#include "dot_matrix.hpp"
namespace web {
// Rendering borrows only physical objects; control state and RNG stay private
// to the simulation. These references are valid for the current synchronous draw.
struct TableMotionView {
    const physics::BallSlots& balls;
    const physics::MechanismState& mechanisms;
    TableMotionView(const physics::SimulationState& state) :
        balls(state.balls),
        mechanisms(state.mechanisms) {}
    TableMotionView(const physics::BallSlots& balls, const physics::MechanismState& mechanisms) :
        balls(balls),
        mechanisms(mechanisms) {}
};
struct TableFrameView {
    TableMotionView motion;
    std::span<const std::int32_t, 79> lamps;
    const DotMatrixFrame& display;
    std::int32_t live_balls;
    bool lamp_animation;
};
} // namespace web
