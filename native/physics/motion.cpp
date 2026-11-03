#include "motion.hpp"
#include "integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web::physics {
using namespace integer;
namespace {
void sound(PhysicsServices services, BallSlots& balls, std::size_t slot, int id, int b) {
    services.events.on_event(
        {
            slot,
            SoundRequest{id, balls.at(slot).position().x < 0xa00000 ? -64 : 64, b, 15}
    },
        balls);
}
void apply_force(BallState& ball, DirectionLookup direction) {
    auto position = ball.position();
    constexpr int magnet_x = 55, magnet_y = 329, capture_half_extent = 40;
    constexpr int force_scale = 4096, velocity_divisor = 0x8100;
    int dx = magnet_x - (position.x >> fixed_point::position_fraction_bits), dy = magnet_y - (position.y >> fixed_point::position_fraction_bits);
    if (dx < -capture_half_extent || dx > capture_half_extent || dy < -capture_half_extent || dy > capture_half_extent)
        return;
    const auto angle = direction({dy, dx});
    // x87 FSQRT then truncation in 0x4027cc; squared distance is at most 3200.
    int distance = 0;
    while ((distance + 1) * (distance + 1) <= dx * dx + dy * dy)
        ++distance;
    if (distance < 1)
        distance = 256;
    if (distance < 2)
        distance = 16;
    if (distance < 4)
        distance = 8;
    const auto force = force_scale / distance;
    auto velocity = ball.velocity();
    velocity.x = add(velocity.x, mul(force, AngleLookup::cosine(angle)) / fixed_point::trig_one);
    velocity.y = add(velocity.y, mul(force, AngleLookup::sine(angle)) / fixed_point::trig_one);
    // SHL discards upper bits before IDIV; do not widen this numerator.
    ball.velocity({mul(velocity.x, fixed_point::trig_one) / velocity_divisor, mul(velocity.y, fixed_point::trig_one) / velocity_divisor});
}
} // namespace
void BallMotion::step(BallSlots& balls, std::size_t slot, MotionContext context, PhysicsServices services) const {
    auto& ball = balls.at(slot);
    const auto type = ball.motion_type();
    if (ball.active() == 1 && (type == physics::ball_type::free || type == physics::ball_type::launcher || type == physics::ball_type::legacy_mobile)) {
        switch (ball.motion_mode()) {
            case physics::motion_mode::table: {
                auto position = ball.position();
                auto velocity = ball.velocity(), acceleration = ball.acceleration();
                ball.previous_position(position);
                velocity = {add(velocity.x, acceleration.x), add(sub(velocity.y, 16), acceleration.y)};
                ball.velocity(velocity);
                ball.position({add(position.x, add(velocity.x, context.nudge.x)), add(position.y, sub(velocity.y, context.nudge.y))});
                ball.z(0);
                break;
            }
            case physics::motion_mode::draining:
                ball.vertical_velocity(sub(ball.vertical_velocity(), 32));
                ball.z(add(ball.z(), ball.vertical_velocity()));
                if ((ball.transition_kind() == 2 && ball.z() < -0x140000) || ball.z() < -0x3c0000)
                    services.events.on_event({slot, DrainContact{}}, balls);
                break;
            case physics::motion_mode::held:
                ball.z(0);
                break;
            case physics::motion_mode::lift:
                ball.z(add(ball.z(), 0x8000));
                if (ball.z() > 0x230000) {
                    auto position = ball.position();
                    position.x = 0xd90000;
                    ball.position(position);
                    auto velocity = ball.velocity();
                    velocity.y = 2000;
                    ball.velocity(velocity);
                    velocity.x = std::int32_t(services.random.next() & 1023) - 19000;
                    ball.motion_mode(physics::motion_mode::table);
                    ball.velocity(velocity);
                    ball.layer(2);
                }
                break;
            case physics::motion_mode::falling:
                ball.falling_velocity(sub(ball.falling_velocity(), 26));
                ball.falling_height(add(ball.falling_height(), ball.falling_velocity()));
                if (ball.falling_height() < 0) {
                    ball.land_on_table();
                    sound(services, balls, slot, 188, 64);
                }
                break;
            default:
                break;
        }
    }
    // Runs even for an inactive current ball and always changes slot five.
    if (context.control_enabled && (context.actions & input_action::ball_control)) {
        auto& controlled = balls[5];
        const auto position = controlled.position();
        if (!context.control_latched)
            context.control_target = {position.x >> 16, position.y >> 16};
        auto velocity = controlled.velocity();
        velocity.x = mul(add(velocity.x, sub(mul(context.control_target.x, fixed_point::position_one), position.x) / 2048), 15) / 16;
        velocity.y = mul(add(velocity.y, sub(mul(context.control_target.y, fixed_point::position_one), position.y) / 2048), 15) / 16;
        controlled.velocity(velocity);
        context.control_latched = 1;
    } else
        context.control_latched = 0;
    if (context.force_timer != 0 && context.force_timer < 0x18000 && ball.layer() == 0)
        apply_force(ball, services.direction);
}
SurfaceGeometry::SurfaceGeometry(CollisionGeometry collision, std::array<TileMap, 2> heights, std::array<TileMap, 2> switches) :
    data_(std::make_shared<const Data>(Data{std::move(collision), std::move(heights), std::move(switches)})) {
    for (const auto* maps : {&data_->heights, &data_->switches})
        for (const auto& map : *maps)
            if (map.columns != table_geometry::columns || map.rows != table_geometry::rows || map.cells.size() != table_geometry::columns * table_geometry::rows)
                throw std::invalid_argument("Surface geometry must be 320 by 616 samples");
}
SurfaceGeometry SurfaceGeometry::load(const std::filesystem::path& assets) {
    return load(assets, CollisionGeometry::load(assets));
}
SurfaceGeometry SurfaceGeometry::load(const std::filesystem::path& assets, CollisionGeometry collision) {
    return {
        std::move(collision),
        {TileMap::load(assets / "COLLIDE/HEIGHT1.MAP", table_geometry::columns), TileMap::load(assets / "COLLIDE/HEIGHT2.MAP", table_geometry::columns)},
        {TileMap::load(assets / "COLLIDE/SWITCH0.MAP", table_geometry::columns), TileMap::load(assets / "COLLIDE/SWITCH1.MAP", table_geometry::columns)}
    };
}
std::uint8_t SurfaceGeometry::height(std::uint16_t layer, int x, int y) const {
    if (layer < 1 || layer > 2)
        throw std::out_of_range("Surface height layer must be 1 or 2");
    if (y < table_geometry::minimum_y || y > table_geometry::maximum_y)
        throw std::out_of_range("Surface coordinate outside map");
    return data_->heights[layer - 1].sample(x, table_geometry::maximum_y - y);
}
std::uint8_t SurfaceGeometry::switches(std::uint16_t layer, int x, int y) const {
    if (layer > 1)
        throw std::out_of_range("Switch layer must be 0 or 1");
    if (y < table_geometry::minimum_y || y > table_geometry::maximum_y)
        throw std::out_of_range("Switch coordinate outside map");
    return data_->switches[layer].sample(x, table_geometry::maximum_y - y);
}
void BallSurface::step(BallSlots& balls, std::size_t slot, PhysicsServices services) const {
    auto& ball = balls.at(slot);
    const auto layer = ball.layer();
    const auto position = ball.position();
    const int x = position.x >> 16, y = position.y >> 16;
    if (layer > 2)
        throw std::out_of_range("Unrecovered surface layer");
    int switch_byte = layer < 2 ? geometry_.switches(layer, x, y) : 0;
    const auto collision_byte = geometry_.collision(layer, x, y);
    if (layer == 0) {
        ball.surface_height(0);
        ball.acceleration({0, 0});
    } else {
        const int left = geometry_.height(layer, x - 3, y), right = geometry_.height(layer, x + 3, y);
        const int down = geometry_.height(layer, x, y - 3), up = geometry_.height(layer, x, y + 3);
        const int height = geometry_.height(layer, x, y);
        ball.surface_height((255 - height) * (layer == 1 ? 0x6d0000 : 0x630000) / 0x9f6);
        int dx = right - left, dy = up - down;
        if (dx > 64 || dx < -64 || dy > 64 || dy < -64)
            ball.acceleration({0, 0});
        else {
            if (layer == 1) {
                dx = std::clamp(dx, -7, 7);
                dy = std::clamp(dy, -7, 7);
            }
            const auto coefficient = layer == 1 ? 0x345 : 0x2f8;
            ball.acceleration({dx * coefficient / 256, dy * coefficient / 256});
        }
    }
    if (switch_byte != 0) {
        if (switch_byte < 0xe0)
            services.events.on_event({slot, SwitchContact{switch_byte - 128}}, balls);
        else
            services.events.on_event({slot, SurfaceContact{switch_byte - 224}}, balls);
    }
    // Callbacks may alter state. Re-read the fields the original re-reads.
    if (collision_byte == 15) {
        if (ball.layer() == 1 && ball.position().y < 0x1000000) {
            if (ball.motion_mode() != physics::motion_mode::falling) {
                const auto vx = std::int32_t(services.random.next() & 1023) - 512;
                const auto vy = std::int32_t(services.random.next() & 1023) - 512;
                ball.begin_surface_fall({vx, vy});
                sound(services, balls, slot, 189, 0);
            }
        } else {
            if (ball.layer() == 2)
                sound(services, balls, slot, 188, ball.position().x < 0xa00000 ? 32 : 64);
            ball.layer(0);
        }
    }
    if (collision_byte == 14)
        ball.layer(1);
    if (collision_byte == 13)
        ball.layer(2);
}
BallPhysics BallPhysics::load(const std::filesystem::path& assets, RepairRecovery recovery) {
    return load(assets, CollisionResponse::load(assets), recovery);
}
BallPhysics BallPhysics::load(const std::filesystem::path& assets, const CollisionResponse& response, RepairRecovery recovery) {
    auto collision = CollisionGeometry::load(assets);
    // Finish sharing the classifier's geometry before transferring the loader's owner.
    CollisionClassifier classifier(collision, recovery);
    BallSurface surface(SurfaceGeometry::load(assets, std::move(collision)));
    return {std::move(classifier), response, std::move(surface)};
}
void BallPhysics::step(BallSlots& balls, std::size_t slot, MotionContext context, PhysicsServices services) const {
    auto& ball = balls.at(slot);
    if (ball.active() != 1)
        return;
    motion_.step(balls, slot, context, services);
    const auto contact = classifier_.classify(ball);
    if (contact != -1)
        response_.reflect(ball, std::uint16_t(contact), context.nudge, services.direction);
    if (ball.motion_mode() == physics::motion_mode::table)
        surface_.step(balls, slot, services);
}
} // namespace web::physics
