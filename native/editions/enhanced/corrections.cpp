#include "corrections.hpp"
#include "../../physics/integer.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace web::enhanced {
namespace {
using namespace physics;
std::uint16_t direction(Vector2 velocity) {
    if (velocity.x == std::numeric_limits<int>::min() || velocity.y == std::numeric_limits<int>::min())
        return AngleLookup::direction(velocity);
    auto x = std::abs(velocity.x), y = std::abs(velocity.y);
    if (x < y) {
        while (x >= 0x3fff8) {
            x >>= 1;
            y >>= 1;
        }
    } else if (x > y) {
        while (y > 0x3fff8) {
            x >>= 1;
            y >>= 1;
        }
    }
    if (x != y)
        return AngleLookup::direction(velocity);
    int angle = physics::angle::eighth_turn;
    if (velocity.x < 0)
        angle = physics::angle::full_turn - angle;
    if (velocity.y < 0)
        angle = physics::angle::half_turn - angle;
    return std::uint16_t(angle);
}
bool usable(const CollisionGeometry& geometry, unsigned layer, Position2 position) {
    const auto x = position.x >> 16, y = position.y >> 16;
    if (x < 0 || x >= table_geometry::width || y < table_geometry::minimum_y || y > table_geometry::maximum_y)
        return false;
    const auto cell = geometry.sample(std::uint16_t(layer), x, y);
    return cell < 0x10 || cell >= 0x18;
}
void recover_collision(const CollisionGeometry& geometry, BallState& ball) {
    auto position = ball.previous_position();
    if (!usable(geometry, ball.layer(), position)) {
        // Stable nearest-cell fallback when the previous point also needs repair.
        // Scan order resolves ties without consuming the game RNG.
        const auto origin = ball.position();
        int distance = std::numeric_limits<int>::max();
        bool found = false;
        for (int y = table_geometry::minimum_y; y <= table_geometry::maximum_y; ++y)
            for (int x = 0; x < table_geometry::width; ++x) {
                const auto candidate_distance = std::abs(x - (origin.x >> 16)) + std::abs(y - (origin.y >> 16));
                if (candidate_distance >= distance)
                    continue;
                const Position2 candidate{x * fixed_point::position_one + fixed_point::position_one / 2, y * fixed_point::position_one + fixed_point::position_one / 2};
                if (usable(geometry, ball.layer(), candidate)) {
                    position = candidate;
                    distance = candidate_distance;
                    found = true;
                }
            }
        if (!found)
            throw std::runtime_error("Collision map has no usable recovery cell");
    }
    ball.position(position);
    const auto velocity = ball.velocity();
    ball.velocity({integer::sub(0, velocity.x), integer::sub(0, velocity.y)});
}
bool flipper_eligible(const BallState& ball) {
    // The three flippers occupy the base playfield. Table motion resets local Z
    // to zero; surface_height distinguishes elevated tracks from that plane.
    return ball.motion_mode() == motion_mode::table && ball.layer() == 0 && ball.z() == 0 && ball.surface_height() == 0 && ball.motion_type() != ball_type::pending_drain;
}
std::optional<std::size_t> launcher(const BallSlots& balls) {
    for (std::size_t slot = 0; slot < balls.size(); ++slot) {
        const auto& ball = balls[slot];
        if (ball.active() == 1 && ball.transition_kind() == 16 && ball.motion_type() == ball_type::launcher && ball.motion_mode() == motion_mode::table && ball.layer() == 0 && ball.z() == 0)
            return slot;
    }
    return std::nullopt;
}
void mission_selection(rules::Player& player) {
    player.missions.all_played = 1;
}
void video_queue(rules::VideoModeState& state) {
    state.pending_shot = state.queued_shot == 1 || state.queued_shot == 2 ? state.queued_shot : 0;
    state.queued_shot = 0;
}
void physical_sound(SoundRequest& request, const BallState& ball) {
    if (request.id == 162 && (ball.transition_kind() == 6 || ball.transition_kind() == 7))
        request.pan = ball.transition_kind() == 6 ? -64 : 64;
}
unsigned nudge_step(NudgeState& state, std::uint16_t actions, const std::function<void()>& on_tilt) {
    const auto requests = update_nudge(state, actions, on_tilt);
    // Recentring resets the bounded push envelope, without moving the balls
    // back or giving collisions a fictitious return velocity.
    if (!state.positive_held && !state.negative_held)
        state.vx = 0;
    if (!state.up_held)
        state.vy = 0;
    return requests;
}
void initialize_mechanisms(MechanismState& state) {

    const auto tip_x = [](const FlipperState& flipper) { return flipper.pivot_x + flipper.length * AngleLookup::sine(std::uint16_t(flipper.rest_angle)) / fixed_point::trig_one; };
    // Reduce the resting gap between the rounded lower tips by ten percent,
    // translating each complete flipper equally in the original 8.8 units.
    const auto gap = tip_x(state.flippers[1]) - state.flippers[1].tip_radius - tip_x(state.flippers[0]) - state.flippers[0].tip_radius;
    constexpr int gap_reduction_denominator = 20;
    const auto inward = (gap + gap_reduction_denominator / 2) / gap_reduction_denominator;
    for (unsigned i = 0; i < 2; ++i) {
        auto& flipper = state.flippers[i];
        const auto offset = i == 0 ? inward : -inward;
        for (auto field : {&FlipperState::pivot_x, &FlipperState::world_x_min, &FlipperState::world_x_max})
            flipper.*field += offset;
    }
}
} // namespace
GameCorrections corrections() {
    return {
        {direction, recover_collision, flipper_eligible, nudge_step},
        launcher,
        mission_selection,
        video_queue,
        physical_sound,
        true,
        true,
        initialize_mechanisms
    };
}
void normalize_sound(SoundRequest& request) {
    request.pan = std::clamp(request.pan, -127, 127);
}
} // namespace web::enhanced
