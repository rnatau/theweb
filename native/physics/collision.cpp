#include "collision.hpp"
#include "../compatibility/physics_metadata.hpp"
#include "integer.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <unordered_set>

namespace web::physics {
using namespace integer;
BallSlots initial_balls() {
    BallSlots balls;
    constexpr std::int32_t first_ball_x = 153, ball_spacing = 32, initial_y = 0x207d00;
    for (unsigned i = 0; i < balls.size(); ++i) {
        balls[i].position({(first_ball_x + std::int32_t(i) * ball_spacing) * fixed_point::position_one, initial_y});
        compatibility::PhysicsMetadata::initialize_unused(balls[i]);
    }
    return balls;
}
CollisionGeometry::CollisionGeometry(std::array<TileMap, 3> layers) :
    layers_(std::make_shared<const std::array<TileMap, 3>>(std::move(layers))) {
    for (const auto& layer : *layers_)
        if (layer.columns != table_geometry::columns || layer.rows != table_geometry::rows || layer.cells.size() != table_geometry::columns * table_geometry::rows)
            throw std::invalid_argument("Collision geometry must be 320 by 616 samples");
}
CollisionGeometry CollisionGeometry::load(const std::filesystem::path& assets) {
    return CollisionGeometry(
        {TileMap::load(assets / "COLLIDE/COLLIDE0.MAP", table_geometry::columns),
         TileMap::load(assets / "COLLIDE/COLLIDE1.MAP", table_geometry::columns),
         TileMap::load(assets / "COLLIDE/COLLIDE2.MAP", table_geometry::columns)});
}
std::uint8_t CollisionGeometry::sample(std::uint16_t layer, int x, int y) const {
    if (layer >= layers_->size())
        throw std::out_of_range("Unrecovered collision layer");
    // Wrappers 0x4181bc / 0x4181e0 / 0x418270 invert physics Y.
    if (x < 0 || x >= table_geometry::width || y < table_geometry::minimum_y || y > table_geometry::maximum_y)
        throw std::out_of_range("Collision coordinate outside map");
    return (*layers_)[layer].sample(x, table_geometry::maximum_y - y);
}

namespace {
// Recovered from the original x87 initializer, including its constants/rounding.
// Reproduce with tools/recover-angle-table.py; no EXE is needed at runtime.
constexpr std::array<std::int16_t, 8192> angles = {
#include "angle_table.inc"
};
constexpr std::array<std::int16_t, 8192> sines = {
#include "sine_table.inc"
};
int lookup(int index) {
    if (index >= int(angles.size()))
        throw std::out_of_range("Original angle normalization would read past its table");
    return angles[std::size_t(index)];
}
} // namespace
std::span<const std::int16_t> AngleLookup::table() {
    return angles;
}
std::span<const std::int16_t> AngleLookup::sine_table() {
    return sines;
}
std::int32_t AngleLookup::sine(std::uint16_t angle) {
    return sines[angle >> 3];
}
std::uint16_t AngleLookup::direction(Vector2 velocity) {
    if (velocity.x == std::numeric_limits<std::int32_t>::min() || velocity.y == std::numeric_limits<std::int32_t>::min())
        throw std::out_of_range("Original angle helper cannot take absolute INT32_MIN safely");
    auto x = velocity.x < 0 ? -velocity.x : velocity.x;
    auto y = velocity.y < 0 ? -velocity.y : velocity.y;
    int angle;
    if (x < y) {
        while (x >= 0x3fff8) {
            x >>= 1;
            y >>= 1;
        }
        angle = x * 8192 > y ? lookup(x * 8192 / y) : 0;
    } else if (x == y)
        angle = physics::angle::eighth_turn; // Includes the original's (0,0) case.
    else {
        while (y > 0x3fff8) {
            x >>= 1;
            y >>= 1;
        }
        angle = y * 8192 > x ? physics::angle::quarter_turn - lookup(y * 8192 / x) : physics::angle::quarter_turn;
    }
    if (velocity.x < 0)
        angle = physics::angle::full_turn - angle;
    if (velocity.y < 0)
        angle = physics::angle::half_turn - angle;
    return std::uint16_t(angle);
}
CollisionResponse CollisionResponse::load(const std::filesystem::path& assets) {
    auto load = [&](const char* name) {
        auto raw = read_file(assets / "GENERAL" / name);
        if (raw.size() != 512)
            throw std::runtime_error("Invalid BVEC response table size");
        Coefficients result;
        for (std::size_t i = 0; i < result.size(); i++)
            result[i] = std::bit_cast<std::int16_t>(u16(raw, 2 * i));
        return result;
    };
    return {load("BVEC0_10.TBL"), load("BVEC0_50.TBL")};
}
void CollisionResponse::reflect(BallState& ball, std::uint16_t contact, Vector2 nudge, DirectionLookup direction) const {
    auto velocity = ball.velocity();
    velocity = {add(velocity.x, nudge.x), sub(velocity.y, nudge.y)};
    auto relative = (unsigned(direction(velocity)) / 256 - contact) & 255;
    if (relative < 64 || relative > 192) {
        const auto& coefficients = (contact & 0x300) ? data_->high : data_->low;
        const auto offset = std::size_t((contact & 0x7f) / 2) * 4;
        const auto a = coefficients[offset], b = coefficients[offset + 1], c = coefficients[offset + 2], d = coefficients[offset + 3];
        const bool scaled = velocity.x > 32767 || velocity.x < -32767 || velocity.y > 32767 || velocity.y < -32767;
        auto x = scaled ? velocity.x / 4 : velocity.x, y = scaled ? velocity.y / 4 : velocity.y;
        const auto divisor = scaled ? 8192 : 32768;
        velocity = {add(mul(a, x), mul(b, y)) / divisor, add(mul(c, x), mul(d, y)) / divisor};
    }
    ball.velocity({sub(velocity.x, nudge.x), add(velocity.y, nudge.y)});
}
int CollisionClassifier::classify(BallState& ball) const {
    auto position = ball.position();
    int x = position.x >> 16, y = position.y >> 16;
    std::unordered_set<unsigned> visited;
    // Original assumes a valid escape path. Bound corrupt/cyclic geometry here.
    for (unsigned repairs = 0; repairs < 320 * 616; repairs++) {
        auto sample = geometry_.sample(ball.layer(), x, y);
        if (sample < 0x10 || sample >= 0x18) {
            if (sample >= 0x40 && sample < 0xc0)
                return (sample & 0x3f) * 4 + (sample > 0x7f ? 0x100 : 0);
            return -1;
        }
        if (!visited.insert(unsigned(y + 1) * 320 + unsigned(x)).second) {
            if (recovery_) {
                recovery_(geometry_, ball);
                const auto repaired = ball.position();
                const auto cell = geometry_.sample(ball.layer(), repaired.x >> 16, repaired.y >> 16);
                if (cell >= 0x10 && cell < 0x18)
                    throw std::runtime_error("Collision recovery must choose a usable cell");
                return -1; // Recovery supplies its own rollback/velocity response.
            }
            throw std::runtime_error("Collision repair entered a cycle");
        }
        const auto direction = sample - 0x10;
        if (direction < 2 || direction > 6) {
            --y;
            position.y = bits((std::uint32_t(position.y) & 0xffff0000) - 1);
        }
        if (direction > 0 && direction < 4) {
            --x;
            position.x = bits((std::uint32_t(position.x) & 0xffff0000) - 1);
        }
        if (direction > 2 && direction < 6) {
            ++y;
            position.y = bits((std::uint32_t(position.y) & 0xffff0000) + 0x10000);
        }
        if (direction > 4 && direction < 8) {
            ++x;
            position.x = bits((std::uint32_t(position.x) & 0xffff0000) + 0x10000);
        }
        ball.position(position);
    }
    throw std::runtime_error("Collision repair did not reach a usable cell");
}
} // namespace web::physics
