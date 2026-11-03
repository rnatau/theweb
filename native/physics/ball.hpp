#pragma once
#include "../web.hpp"
#include "units.hpp"

namespace web::compatibility {
struct PhysicsSnapshot;
struct PhysicsMetadata;
} // namespace web::compatibility
namespace web::physics {
enum class MotionMode : std::uint16_t {
    table = 0,
    draining = 1,
    held = 2,
    lift = 3,
    falling = 4,
    station_hold = 5
};
enum class BallType : std::uint16_t {
    none = 0,
    free = 1,
    launcher = 2,
    centre_return = 3,
    upper_return = 4,
    lift_return = 5,
    left_return = 6,
    pending_drain = 7,
    legacy_mobile = 8,
    station_capture = 9
};
namespace motion_mode {
using enum MotionMode;
}
namespace ball_type {
using enum BallType;
// Type eight participates in original movement/drawing gates; its shot role is
// unconfirmed. Preserve that membership without inventing a table mechanism.
} // namespace ball_type
struct Vector2 {
    std::int32_t x = 0, y = 0;
    bool operator==(const Vector2&) const = default;
};
// Q16 table coordinates are positions, not velocity/force vectors. Arithmetic
// remains explicit at each recovered operation so integer rounding is retained.
struct Position2 {
    std::int32_t x = 0, y = 0;
    bool operator==(const Position2&) const = default;
};

// Ball motion is ordinary typed state. Historical record layouts belong to the
// compatibility snapshot codec, including bytes with no confirmed gameplay role.
class BallState {
public:
    BallState() = default;
    Position2 position() const {
        return {position_.x, position_.y};
    }
    Vector2 velocity() const {
        return {velocity_.x, velocity_.y};
    }
    void position(Position2 value) {
        position_.x = value.x;
        position_.y = value.y;
    }
    void velocity(Vector2 value) {
        velocity_.x = value.x;
        velocity_.y = value.y;
    }
    std::int32_t z() const {
        return z_;
    }
    void z(std::int32_t value) {
        z_ = value;
    }
    std::int32_t vertical_velocity() const {
        return vertical_velocity_;
    }
    void vertical_velocity(std::int32_t value) {
        vertical_velocity_ = value;
    }
    Vector2 acceleration() const {
        return {acceleration_.x, acceleration_.y};
    }
    void acceleration(Vector2 value) {
        acceleration_.x = value.x;
        acceleration_.y = value.y;
    }
    Position2 previous_position() const {
        return {previous_position_.x, previous_position_.y};
    }
    void previous_position(Position2 value) {
        previous_position_.x = value.x;
        previous_position_.y = value.y;
    }
    Position2 local_position() const {
        return {local_position_.x, local_position_.y};
    }
    void local_position(Position2 value) {
        local_position_.x = value.x;
        local_position_.y = value.y;
    }
    std::int32_t surface_height() const {
        return surface_height_;
    }
    void surface_height(std::int32_t value) {
        surface_height_ = value;
    }
    std::int32_t falling_height() const {
        return falling_height_;
    }
    void falling_height(std::int32_t value) {
        falling_height_ = value;
    }
    std::int32_t falling_velocity() const {
        return falling_velocity_;
    }
    void falling_velocity(std::int32_t value) {
        falling_velocity_ = value;
    }
    std::int32_t transition_kind() const {
        return transition_kind_;
    }
    void transition_kind(std::int32_t value) {
        transition_kind_ = value;
    }
    std::int32_t rule_timer() const {
        return rule_timer_;
    }
    void rule_timer(std::int32_t value) {
        rule_timer_ = value;
    }
    std::uint16_t active() const {
        return active_;
    }
    void active(std::uint16_t value) {
        active_ = value;
    }
    BallType motion_type() const {
        return motion_type_;
    }
    void motion_type(BallType value) {
        motion_type_ = value;
    }
    MotionMode motion_mode() const {
        return motion_mode_;
    }
    void motion_mode(MotionMode value) {
        motion_mode_ = value;
    }
    std::uint16_t contact_retry() const {
        return contact_retry_;
    }
    void contact_retry(std::uint16_t value) {
        contact_retry_ = value;
    }
    std::uint16_t layer() const {
        return layer_;
    }
    void layer(std::uint16_t value) {
        layer_ = value;
    }
    void place_in_launcher() {
        active_ = 1;
        motion_type_ = BallType::launcher;
        position_ = {0x1260000, 0x6c0000};
        layer_ = 0;
        velocity_ = {};
        // Original serve retains mode, height, timers and previous coordinates.
    }
    void begin_lift() {
        motion_type_ = BallType::free;
        motion_mode_ = MotionMode::lift;
        position_ = {0xdf0000, 0x1590000};
        // The original lift keeps velocity and vertical state until release.
    }
    void return_to_playfield(Position2 position, Vector2 velocity) {
        motion_type_ = BallType::free;
        position_ = position;
        velocity_ = velocity;
        z_ = vertical_velocity_ = 0;
        layer_ = 1;
        motion_mode_ = MotionMode::table;
    }
    void await_drain_removal(std::int32_t duration) {
        motion_type_ = BallType::pending_drain;
        rule_timer_ = duration;
        motion_mode_ = MotionMode::held;
        z_ = 0;
    }
    void capture_in_station(std::int32_t duration) {
        motion_mode_ = MotionMode::station_hold;
        motion_type_ = BallType::station_capture;
        rule_timer_ = duration;
    }
    void begin_surface_fall(Vector2 velocity) {
        velocity_ = velocity;
        falling_velocity_ = -328;
        falling_height_ = surface_height_;
        motion_mode_ = MotionMode::falling;
    }
    void land_on_table() {
        surface_height_ = 0;
        layer_ = 0;
        motion_mode_ = MotionMode::table;
    }

private:
    friend struct web::compatibility::PhysicsSnapshot;
    friend struct web::compatibility::PhysicsMetadata;
    Position2 position_, previous_position_, local_position_;
    Vector2 velocity_, acceleration_;
    std::int32_t z_ = 0, vertical_velocity_ = 0, surface_height_ = 0, falling_height_ = 0, falling_velocity_ = 0, transition_kind_ = 0, rule_timer_ = 0;
    std::uint16_t active_ = 0, contact_retry_ = 0, layer_ = 0;
    BallType motion_type_ = BallType::none;
    MotionMode motion_mode_ = MotionMode::table;
    std::array<std::uint8_t, 79> compatibility_bytes_{};
};
using BallSlots = std::array<BallState, 6>;
BallSlots initial_balls();
} // namespace web::physics
