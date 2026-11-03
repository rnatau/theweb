#include "simulation.hpp"
#include "integer.hpp"
#include <stdexcept>

namespace web::physics {
MotionContext SimulationState::motion_context() {
    return {
        {nudge.vx, nudge.vy},
        actions,
        force_timer,
        control_enabled,
        control_latched,
        control_target
    };
}
PhysicsSimulation PhysicsSimulation::load(const std::filesystem::path& assets, PhysicsPolicy policy) {
    const auto response = CollisionResponse::load(assets);
    return PhysicsSimulation(BallPhysics::load(assets, response, policy.repair_cycle), FlipperContacts(response), policy);
}
unsigned PhysicsSimulation::microstep_count(std::int32_t refresh_rate, std::int32_t double_steps) {
    if (refresh_rate == 0)
        throw std::invalid_argument("Original physics refresh divisor must be nonzero");
    constexpr int microstep_budget_per_second = 1050;
    auto count = microstep_budget_per_second / refresh_rate;
    if (double_steps)
        count = integer::add(count, count);
    return count > 0 ? unsigned(count) : 0;
}
unsigned PhysicsSimulation::step(SimulationState& state, PhysicsEventSink& events, const std::function<void(unsigned, unsigned)>& before_microstep) const {
    const auto count = microstep_count(state.refresh_rate, state.double_steps);
    for (unsigned tick = 0; tick < count; tick++) {
        if (before_microstep)
            before_microstep(tick, count);
        step_microstep(state, events);
    }
    return count;
}
void PhysicsSimulation::step_microstep(SimulationState& state, PhysicsEventSink& events) const {
    policy_.nudge_step(state.nudge, std::uint16_t(state.actions), [&] { events.on_event(web::physics::PhysicsEvent{PhysicsEvent::no_ball, web::physics::TiltContact{}}, state.balls); });
    for (std::size_t slot = 0; slot < state.balls.size(); slot++) {
        auto context = state.motion_context();
        balls_.step(state.balls, slot, context, {state.random, events, policy_.direction});
    }
    drives_.step(state.mechanisms, state.balls, state.actions, {state.random, events});
    spinners_.step(state.mechanisms, state.balls, {state.random, events});
    for (auto& ball : state.balls)
        if (ball.active() == 1 && (!policy_.flipper_eligible || policy_.flipper_eligible(ball)))
            flippers_.step(ball, state.mechanisms, state.contact, {state.nudge.vx, state.nudge.vy}, policy_.direction);
    interactions_.step(state.balls, {state.random, events, policy_.direction});
}
} // namespace web::physics
