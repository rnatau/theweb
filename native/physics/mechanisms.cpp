#include "mechanisms.hpp"
#include "../compatibility/physics_metadata.hpp"
#include "integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web::physics {
using namespace integer;
namespace {
std::int32_t absolute(std::int32_t value) {
    return value < 0 ? sub(0, value) : value;
}
std::uint64_t square_root(std::uint64_t value) {
    // Exact floor sqrt, matching positive x87 FSQRT followed by truncation.
    std::uint64_t result = 0, bit = std::uint64_t(1) << 62;
    while (bit > value)
        bit >>= 2;
    while (bit) {
        if (value >= result + bit) {
            value -= result + bit;
            result = (result >> 1) + bit;
        } else
            result >>= 1;
        bit >>= 2;
    }
    return result;
}
bool eligible(const BallState& ball) {
    return ball.active() == 1 && ball.motion_type() != physics::ball_type::pending_drain && ball.motion_mode() == physics::motion_mode::table;
}
} // namespace
MechanismState MechanismState::defaults() {
    MechanismState state;
    for (auto& spinner : state.spinners)
        compatibility::PhysicsMetadata::initialize_unused(spinner);
    {
        auto& flipper = state.flippers[0];
        flipper.pivot_x = 27540;
        flipper.pivot_y = 26895;
        flipper.angle = 21845;
        flipper.base_contact_radius_squared = 10608049;
        flipper.tip_contact_radius_squared = 7420176;
        flipper.base_radius = 1465;
        flipper.tip_radius = 932;
        flipper.world_x_min = 17489;
        flipper.world_x_max = 37591;
        flipper.world_y_min = 16844;
        flipper.world_y_max = 36946;
        flipper.local_x_max = 10983;
        flipper.local_x_min = -3257;
        flipper.local_y_max = 3257;
        flipper.local_y_min = -3257;
        flipper.length = 8259;
        flipper.slope = 3966;
        flipper.intercept = 48855;
        flipper.rest_angle = 21845;
        flipper.powered_angle = 10922;
        flipper.lower_normal = 33439;
        flipper.upper_normal = 64865;
        flipper.normal_offset_x = 114;
        flipper.normal_offset_y = 1788;
        flipper.direction = -1;
        flipper.return_latch = 1;
        compatibility::PhysicsMetadata::initialize_unused(flipper);
    }
    {
        auto& flipper = state.flippers[1];
        flipper.pivot_x = 50796;
        flipper.pivot_y = 26895;
        flipper.angle = 43690;
        flipper.base_contact_radius_squared = 10608049;
        flipper.tip_contact_radius_squared = 7420176;
        flipper.base_radius = 1465;
        flipper.tip_radius = 932;
        flipper.world_x_min = 40745;
        flipper.world_x_max = 60847;
        flipper.world_y_min = 16844;
        flipper.world_y_max = 36946;
        flipper.local_x_max = 10983;
        flipper.local_x_min = -3257;
        flipper.local_y_max = 3257;
        flipper.local_y_min = -3257;
        flipper.length = 8259;
        flipper.slope = 3966;
        flipper.intercept = 48855;
        flipper.rest_angle = 43690;
        flipper.powered_angle = 54613;
        flipper.lower_normal = 33439;
        flipper.upper_normal = 64865;
        flipper.normal_offset_x = 114;
        flipper.normal_offset_y = 1788;
        flipper.direction = 1;
        flipper.return_latch = 1;
        compatibility::PhysicsMetadata::initialize_unused(flipper);
    }
    {
        auto& flipper = state.flippers[2];
        flipper.pivot_x = 7688;
        flipper.pivot_y = 90510;
        flipper.angle = 29127;
        flipper.base_contact_radius_squared = 10608049;
        flipper.tip_contact_radius_squared = 7420176;
        flipper.base_radius = 1465;
        flipper.tip_radius = 932;
        flipper.world_x_min = -2363;
        flipper.world_x_max = 17739;
        flipper.world_y_min = 80459;
        flipper.world_y_max = 100561;
        flipper.local_x_max = 10983;
        flipper.local_x_min = -3257;
        flipper.local_y_max = 3257;
        flipper.local_y_min = -3257;
        flipper.length = 8259;
        flipper.slope = 3966;
        flipper.intercept = 48855;
        flipper.rest_angle = 29127;
        flipper.powered_angle = 18204;
        flipper.lower_normal = 33439;
        flipper.upper_normal = 64865;
        flipper.normal_offset_x = 114;
        flipper.normal_offset_y = 1788;
        flipper.direction = -1;
        flipper.return_latch = 1;
        compatibility::PhysicsMetadata::initialize_unused(flipper);
    }
    return state;
}
void FlipperDrive::step(MechanismState& state, BallSlots& balls, std::uint32_t actions, PhysicsServices services) const {

    for (auto& flipper : state.flippers) {
        const bool positive = flipper.direction == 1;
        const bool held = state.enabled != 0 && (actions & (positive ? input_action::right_flipper : input_action::left_flipper));
        const auto previous = flipper.angle;
        flipper.previous_angle = previous;
        std::int32_t drive;
        if (held) {
            drive = 1;
            flipper.return_latch = 0;
            const auto angle = add(previous, positive ? 0x130 : -0x130);
            flipper.angle = angle;
            // The negative-direction endpoint uses a strict comparison.
            if (positive ? angle >= flipper.powered_angle : angle < flipper.powered_angle) {
                drive = 0;
                flipper.angle = flipper.powered_angle;
                if (flipper.powered_latch == 0) {
                    flipper.powered_latch = 1;
                    services.events.on_event(
                        web::physics::PhysicsEvent{
                            PhysicsEvent::no_ball,
                            web::SoundRequest{154, positive ? 64 : -64, 0, positive ? 5 : 4}
                    },
                        balls);
                }
            }
        } else {
            drive = -1;
            flipper.powered_latch = 0;
            const auto angle = add(previous, positive ? -0x80 : 0x80);
            flipper.angle = angle;
            if (positive ? angle < flipper.rest_angle : angle > flipper.rest_angle) {
                drive = 0;
                flipper.angle = flipper.rest_angle;
                if (flipper.return_latch == 0)
                    flipper.return_latch = 1;
            }
        }
        // Original commits drive after the sound callback, not before it.
        flipper.drive = drive;
    }
}
void SpinnerDynamics::kick(MechanismState& state, std::size_t spinner, std::int32_t velocity) const {
    auto& current = state.spinners.at(spinner);
    current.velocity = velocity;
    current.phase = 0x40000;
}
void SpinnerDynamics::step(MechanismState& state, BallSlots& balls, PhysicsServices services) const {

    for (std::size_t i = 0; i < state.spinners.size(); i++) {
        auto& spinner = state.spinners[i];
        auto velocity = spinner.velocity;
        velocity = velocity < 0 ? std::min(add(velocity, 2), 0) : std::max(sub(velocity, 2), 0);
        spinner.velocity = velocity;
        const auto previous = spinner.phase, next = add(previous, velocity);
        spinner.phase = next;
        const auto old_frame = (previous >> 15) & 15, new_frame = (next >> 15) & 15;
        if ((old_frame == 0 && new_frame == 15) || (old_frame == 15 && new_frame == 0))
            services.events.on_event(web::physics::PhysicsEvent{PhysicsEvent::no_ball, web::physics::SpinnerCrossing{std::int32_t(i)}}, balls);
    }
}
std::int32_t BallInteractions::resolve(BallSlots& balls, std::size_t first, std::size_t second, DirectionLookup direction) const {
    if (first == second)
        throw std::invalid_argument("Ball contact needs two distinct slots");
    auto& a = balls.at(first);
    auto& b = balls.at(second);
    const auto p = a.position(), q = b.position();
    const auto position_angle = direction({sub(p.x, q.x) / 256, sub(p.y, q.y) / 256});
    auto av = a.velocity(), bv = b.velocity();
    const Vector2 difference{sub(av.x, bv.x), sub(av.y, bv.y)};
    const auto velocity_angle = direction(difference);
    const bool scaled = difference.x > 8192 || difference.x < -8192 || difference.y > 8192 || difference.y < -8192;
    const auto vx = scaled ? difference.x / 256 : difference.x, vy = scaled ? difference.y / 256 : difference.y;
    const auto squared = add(mul(vx, vx), mul(vy, vy));
    if (squared < 0)
        throw std::out_of_range("Original ball contact takes square root of a negative wrapped value");
    auto magnitude = std::min<std::int32_t>(std::int32_t(square_root(std::uint64_t(squared) * (scaled ? 65536 : 1))), 65535);
    const auto delta = int(velocity_angle) - int(position_angle); // Original does not wrap this difference.
    if (delta > 0x4000 || delta < -0x4000) {
        auto coefficient = AngleLookup::cosine(std::uint16_t(delta)) / 2;
        if (magnitude < 64) {
            magnitude = 64;
            coefficient = 0x4000;
        }
        const auto factor = mul(coefficient, coefficient) / 16384;
        const auto impulse = mul(magnitude, factor) / 16384;
        const auto dx = mul(AngleLookup::sine(position_angle) / 2, impulse) / 16384;
        const auto dy = mul(AngleLookup::cosine(position_angle) / 2, impulse) / 16384;
        av = {add(av.x, dx), add(av.y, dy)};
        bv = {sub(bv.x, dx), sub(bv.y, dy)};
        av = {std::clamp(av.x, -65535, 65535), std::clamp(av.y, -65535, 65535)};
        bv = {std::clamp(bv.x, -65535, 65535), std::clamp(bv.y, -65535, 65535)};
        a.velocity(av);
        b.velocity(bv);
    }
    return add(absolute(sub(av.x, bv.x)), absolute(sub(av.y, bv.y)));
}
void BallInteractions::step(BallSlots& balls, PhysicsServices services) const {
    for (unsigned pass = 0; pass < 2; pass++) {
        for (auto& ball : balls)
            ball.contact_retry(0);
        for (std::size_t i = 0; i < 5; i++)
            if (eligible(balls[i])) {
                for (std::size_t j = i + 1; j < 6; j++)
                    if (eligible(balls[j])) {
                        const auto p = balls[i].position(), q = balls[j].position();
                        auto dx = sub(p.x, q.x), dy = sub(p.y, q.y);
                        if (dx >= 0xe0000 || dx <= -0xe0000 || dy >= 0xe0000 || dy <= -0xe0000)
                            continue;
                        dx /= 256;
                        dy /= 256;
                        if (add(mul(dx, dx), mul(dy, dy)) >= 0xc40000 || balls[i].layer() != balls[j].layer())
                            continue;
                        const auto relative = resolve(balls, i, j, services.direction);
                        if (relative > 8192)
                            services.events.on_event(
                                web::physics::PhysicsEvent{
                                    i,
                                    web::SoundRequest{155, 0, 128 - std::min(relative, 16383) / 128, 16}
                            },
                                balls);
                    }
            }
        bool retry = false;
        for (auto& ball : balls)
            if (ball.contact_retry() == 1) {
                ball.position(ball.previous_position());
                retry = true;
            }
        if (!retry)
            break;
    }
}
} // namespace web::physics
