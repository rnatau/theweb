#include "flippers.hpp"
#include "integer.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace web::physics {
using namespace integer;

void FlipperContacts::transform(BallState& ball, Vector2 pivot, std::int32_t angle) {
    const auto position = ball.position();
    const auto x = sub(position.x / 256, pivot.x), y = sub(position.y / 256, pivot.y);
    const auto theta = std::uint16_t(sub(angle, physics::angle::quarter_turn));
    const auto sine = AngleLookup::sine(theta), cosine = AngleLookup::cosine(theta);
    ball.local_position({sub(mul(cosine, x), mul(sine, y)) / fixed_point::trig_one, add(mul(sine, x), mul(cosine, y)) / fixed_point::trig_one});
}
int FlipperContacts::classify(BallState& ball, const FlipperState& flipper, FlipperContactState& contact, DirectionLookup direction) const {
    contact.flag = 0;
    contact.tip_angle = physics::angle::half_turn;
    const auto position = ball.position();
    const auto x = position.x / 256, y = position.y / 256;
    if (y >= flipper.world_y_max || y <= flipper.world_y_min || x >= flipper.world_x_max || x <= flipper.world_x_min)
        return -1;
    transform(ball, {flipper.pivot_x, flipper.pivot_y}, flipper.angle);
    const auto local = ball.local_position();
    if (local.y >= flipper.local_y_max || local.y <= flipper.local_y_min || local.x >= flipper.local_x_max || local.x <= flipper.local_x_min)
        return -1;
    const auto length = flipper.length;
    if (local.x > length || local.x < 0) {
        const bool tip = local.x > length;
        const auto dx = tip ? sub(local.x, length) : local.x;
        if (add(mul(local.y, local.y), mul(dx, dx)) >= (tip ? flipper.tip_contact_radius_squared : flipper.base_contact_radius_squared))
            return -1;
        const auto angle = direction({dx, local.y});
        const auto radius = (tip ? flipper.tip_radius : flipper.base_radius);
        contact.surface_x = add(tip ? length : 0, mul(radius, AngleLookup::sine(angle)) / fixed_point::trig_one);
        contact.surface_y = mul(radius, AngleLookup::cosine(angle)) / fixed_point::trig_one;
        contact.angle = add(angle, physics::angle::half_turn);
        if (tip)
            contact.tip_angle = contact.angle;
    } else {
        const auto slope = mul(flipper.slope, local.y) / 256;
        if ((local.y < 0 ? sub(local.x, slope) : add(local.x, slope)) >= flipper.intercept)
            return -1;
        contact.angle = (local.y < 0 ? flipper.upper_normal : flipper.lower_normal);
        contact.surface_x = sub(local.x, flipper.normal_offset_x);
        contact.surface_y = local.y < 0 ? add(local.y, flipper.normal_offset_y) : sub(local.y, flipper.normal_offset_y);
    }
    return 1;
}
void FlipperContacts::reflect(BallState& ball, const FlipperState& flipper, FlipperContactState& contact, Vector2 nudge, DirectionLookup direction_lookup) const {
    const auto angle = std::uint16_t(add(sub(flipper.angle, physics::angle::quarter_turn), contact.angle));
    const auto drive = flipper.drive;
    response_.reflect(ball, std::uint16_t((angle / 256) + (drive == -1 ? 0 : 0x200)), nudge, direction_lookup);
    if (drive == 0) {
        // Original 0x401b38: raw fixed-point Y correction, not a whole pixel.
        if (angle / 256 > 0x50 && angle / 256 < 0xb0) {
            auto position = ball.position();
            position.y = add(position.y, 16);
            ball.position(position);
        }
        return;
    }
    const auto theta = std::uint16_t(angle - physics::angle::half_turn);
    const auto sine = AngleLookup::sine(theta) / 4, cosine = AngleLookup::cosine(theta) / 4;
    const auto velocity = ball.velocity();
    const auto tangent = sub(mul(cosine, velocity.x), mul(sine, velocity.y)) / 8192;
    auto normal = add(mul(sine, velocity.x), mul(cosine, velocity.y)) / 8192;
    const auto y = ball.local_position().y, direction = flipper.direction;
    const bool powered = drive == 1 && ((y >= 0 && direction == -1) || (y <= 0 && direction == 1));
    const bool returning = (y < 0 && direction == -1) || (y > 0 && direction == 1);
    if (powered || returning) {
        const auto square = add(mul(contact.surface_x, contact.surface_x), mul(contact.surface_y, contact.surface_y));
        if (square < 0)
            throw std::domain_error("Flipper radius squared overflows original signed sqrt domain");
        // Original x87 CW 0x127f: double precision FSQRT, two FMULs, then
        // truncation helper 0x42257e. Constants at 0x42a094/9c/a4 are exact.
        const double scaled = std::sqrt(double(square)) * (powered ? 30561.0 : 12867.0);
        auto minimum = std::int32_t(scaled * 0x1p-12);
        if (contact.tip_angle != physics::angle::half_turn) {
            auto delta = sub(contact.tip_angle, (physics::angle::half_turn + physics::angle::quarter_turn));
            if (delta < 0)
                delta = sub(0, delta);
            contact.tip_angle = delta;
            minimum = mul(minimum, delta) / 16384;
        }
        normal = std::max(normal, minimum);
    }
    ball.velocity({add(mul(cosine, tangent), mul(sine, normal)) / 8192, sub(mul(cosine, normal), mul(sine, tangent)) / 8192});
}
void FlipperContacts::step(BallState& ball, const MechanismState& mechanisms, FlipperContactState& contact, Vector2 nudge, DirectionLookup direction) const {
    for (const auto& flipper : mechanisms.flippers) {
        contact.probe = 0;
        if (classify(ball, flipper, contact, direction) != -1)
            reflect(ball, flipper, contact, nudge, direction);
    }
}
} // namespace web::physics
