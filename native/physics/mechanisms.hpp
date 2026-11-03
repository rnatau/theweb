#pragma once
#include "motion.hpp"

namespace web::physics {
class FlipperState {
public:
    FlipperState() = default;
    std::int32_t pivot_x = 0;
    std::int32_t pivot_y = 0;
    std::int32_t angle = 0;
    std::int32_t previous_angle = 0;
    std::int32_t base_contact_radius_squared = 0;
    std::int32_t tip_contact_radius_squared = 0;
    std::int32_t base_radius = 0;
    std::int32_t tip_radius = 0;
    std::int32_t world_x_min = 0;
    std::int32_t world_x_max = 0;
    std::int32_t world_y_min = 0;
    std::int32_t world_y_max = 0;
    std::int32_t local_x_max = 0;
    std::int32_t local_x_min = 0;
    std::int32_t local_y_max = 0;
    std::int32_t local_y_min = 0;
    std::int32_t length = 0;
    std::int32_t slope = 0;
    std::int32_t intercept = 0;
    std::int32_t rest_angle = 0;
    std::int32_t powered_angle = 0;
    std::int32_t lower_normal = 0;
    std::int32_t upper_normal = 0;
    std::int32_t normal_offset_x = 0;
    std::int32_t normal_offset_y = 0;
    std::int32_t direction = 0;
    std::int32_t drive = 0;
    std::int32_t return_latch = 0;
    std::int32_t powered_latch = 0;

private:
    friend struct web::compatibility::PhysicsSnapshot;
    friend struct web::compatibility::PhysicsMetadata;
    std::array<std::uint8_t, 32> compatibility_bytes_{};
};
class SpinnerState {
public:
    SpinnerState() = default;
    std::int32_t velocity = 0;
    std::int32_t phase = 0;

private:
    friend struct web::compatibility::PhysicsSnapshot;
    friend struct web::compatibility::PhysicsMetadata;
    std::array<std::uint8_t, 16> compatibility_bytes_{};
};
struct MechanismState {
    std::array<SpinnerState, 2> spinners;
    std::array<FlipperState, 3> flippers;
    std::int32_t enabled = 0;
    static MechanismState defaults(); // Original initializers; opaque asset handles are zero.
};
class FlipperDrive {
public:
    // Original 0x403bdc -> 0x4039f8; only drive state, not contact geometry/impulse.
    void step(MechanismState& state, BallSlots& balls, std::uint32_t actions, PhysicsServices services) const;
};
class SpinnerDynamics {
public:
    // Original 0x404260 and 0x404304 -> 0x404280.
    void kick(MechanismState& state, std::size_t spinner, std::int32_t velocity) const;
    void step(MechanismState& state, BallSlots& balls, PhysicsServices services) const;
};
class BallInteractions {
public:
    // Original 0x402fc8. Caller is responsible for broad-phase eligibility.
    std::int32_t resolve(BallSlots& balls, std::size_t first, std::size_t second, DirectionLookup direction = AngleLookup::direction) const;
    // Original 0x4032e4: ordered same-layer pairs, audio and at most two retry passes.
    void step(BallSlots& balls, PhysicsServices services) const;
};
} // namespace web::physics
