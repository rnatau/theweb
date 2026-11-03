#pragma once
#include "mechanisms.hpp"

namespace web::physics {
// Shared scratch at original VAs 0x430470..0x430480 and 0x42c268.
// Keep fields after a miss: the original resets only flag and tip_angle.
struct FlipperContactState {
    std::int32_t angle = 0, surface_y = 0, surface_x = 0, flag = 0, tip_angle = 0, probe = 0;
};
class FlipperContacts {
public:
    explicit FlipperContacts(CollisionResponse response) :
        response_(std::move(response)) {}
    static FlipperContacts load(const std::filesystem::path& assets) {
        return FlipperContacts(CollisionResponse::load(assets));
    }
    // Original 0x402c7c; stores the transformed position in the ball record.
    static void transform(BallState& ball, Vector2 pivot, std::int32_t angle);
    // Original 0x403bf0; no active/mode/layer filtering within this routine.
    int classify(BallState& ball, const FlipperState& flipper, FlipperContactState& contact, DirectionLookup direction = AngleLookup::direction) const;
    // Original 0x403ea0; consumes scratch from a successful classification.
    void reflect(BallState& ball, const FlipperState& flipper, FlipperContactState& contact, Vector2 nudge = {}, DirectionLookup direction = AngleLookup::direction) const;
    // Original 0x4041d0: all three flippers in order, including scratch resets.
    void step(BallState& ball, const MechanismState& mechanisms, FlipperContactState& contact, Vector2 nudge = {}, DirectionLookup direction = AngleLookup::direction) const;

private:
    CollisionResponse response_;
};
} // namespace web::physics
