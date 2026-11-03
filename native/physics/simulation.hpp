#pragma once
#include "flippers.hpp"
#include "nudge.hpp"

namespace web::physics {
// One explicit owner for recovered mutable physics state. Inputs and pacing are
// supplied by the caller; no window timer, CD or original executable is involved.
struct SimulationState {
    BallSlots balls;
    MechanismState mechanisms = MechanismState::defaults();
    NudgeState nudge;
    FlipperContactState contact;
    RandomGenerator random{1};
    std::uint32_t actions = 0;
    std::int32_t force_timer = 0, control_enabled = 0, control_latched = 0;
    Vector2 control_target;
    std::int32_t refresh_rate = 60, double_steps = 0;
    MotionContext motion_context();
};
class PhysicsSimulation {
public:
    PhysicsSimulation(BallPhysics balls, FlipperContacts flippers, PhysicsPolicy policy = {}) :
        balls_(std::move(balls)),
        flippers_(std::move(flippers)),
        policy_(policy) {}
    static PhysicsSimulation load(const std::filesystem::path& assets, PhysicsPolicy policy = {});
    // Original 0x4146e0: signed 1050/refresh_rate, optionally doubled. Captured
    // once per call; nonpositive counts perform no work. Zero divisor rejected.
    static unsigned microstep_count(std::int32_t refresh_rate, std::int32_t double_steps);
    void step_microstep(SimulationState& state, PhysicsEventSink& events) const;
    unsigned step(SimulationState& state, PhysicsEventSink& events, const std::function<void(unsigned, unsigned)>& before_microstep = {}) const;

private:
    BallPhysics balls_;
    FlipperContacts flippers_;
    FlipperDrive drives_;
    SpinnerDynamics spinners_;
    BallInteractions interactions_;
    PhysicsPolicy policy_;
};
} // namespace web::physics
