#pragma once
#include "../physics/mechanisms.hpp"
#include <algorithm>
#include <stdexcept>
namespace web::compatibility {
inline constexpr std::size_t ball_snapshot_bytes = 157, flipper_snapshot_bytes = 148, spinner_snapshot_bytes = 24, mechanism_snapshot_bytes = 496;
struct PhysicsSnapshot {
    template <class State> static State read(std::span<const std::uint8_t> bytes) {
        State state;
        decode(state, bytes);
        return state;
    }
    static void decode(physics::MechanismState& state, std::span<const std::uint8_t> bytes) {
        if (bytes.size() != mechanism_snapshot_bytes)
            throw std::invalid_argument("Invalid mechanism state length");
        for (std::size_t i = 0; i < state.spinners.size(); ++i)
            decode(state.spinners[i], bytes.subspan(i * spinner_snapshot_bytes, spinner_snapshot_bytes));
        for (std::size_t i = 0; i < state.flippers.size(); ++i)
            decode(state.flippers[i], bytes.subspan(48 + i * flipper_snapshot_bytes, flipper_snapshot_bytes));
        state.enabled = word(bytes, 492);
    }
    static Bytes encode(const physics::MechanismState& state) {
        Bytes result;
        result.reserve(mechanism_snapshot_bytes);
        for (const auto& spinner : state.spinners) {
            const auto bytes = encode(spinner);
            result.insert(result.end(), bytes.begin(), bytes.end());
        }
        for (const auto& flipper : state.flippers) {
            const auto bytes = encode(flipper);
            result.insert(result.end(), bytes.begin(), bytes.end());
        }
        result.resize(mechanism_snapshot_bytes);
        store(result, 492, state.enabled);
        return result;
    }
    static std::int32_t word(std::span<const std::uint8_t> bytes, std::size_t offset) {
        return std::bit_cast<std::int32_t>(u32(bytes, offset));
    }
    static void store(Bytes& bytes, std::size_t offset, std::int32_t value, unsigned width = 4) {
        for (unsigned i = 0; i < width; ++i)
            bytes[offset + i] = std::uint8_t(std::uint32_t(value) >> (8 * i));
    }
    static void decode(physics::BallState& state, std::span<const std::uint8_t> bytes) {
        if (bytes.size() != 157)
            throw std::invalid_argument("Invalid BallState snapshot length");
        state.position_.x = word(bytes, 0);
        state.position_.y = word(bytes, 4);
        state.z_ = word(bytes, 8);
        state.velocity_.x = word(bytes, 20);
        state.velocity_.y = word(bytes, 24);
        state.vertical_velocity_ = word(bytes, 28);
        state.acceleration_.x = word(bytes, 32);
        state.acceleration_.y = word(bytes, 36);
        state.previous_position_.x = word(bytes, 40);
        state.previous_position_.y = word(bytes, 44);
        state.local_position_.x = word(bytes, 48);
        state.local_position_.y = word(bytes, 52);
        state.surface_height_ = word(bytes, 76);
        state.falling_height_ = word(bytes, 80);
        state.falling_velocity_ = word(bytes, 84);
        state.transition_kind_ = word(bytes, 88);
        state.rule_timer_ = word(bytes, 92);
        state.active_ = u16(bytes, 140);
        state.motion_type_ = physics::BallType(u16(bytes, 144));
        state.motion_mode_ = physics::MotionMode(u16(bytes, 146));
        state.contact_retry_ = u16(bytes, 148);
        state.layer_ = u16(bytes, 152);
        constexpr std::array<std::size_t, 79> offsets = {12,  13,  14,  15,  16,  17,  18,  19,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
                                                         75,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
                                                         122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 142, 143, 150, 151, 154, 155, 156};
        for (std::size_t i = 0; i < offsets.size(); ++i)
            state.compatibility_bytes_[i] = bytes[offsets[i]];
    }
    static Bytes encode(const physics::BallState& state) {
        Bytes bytes(157);
        store(bytes, 0, state.position_.x, 4);
        store(bytes, 4, state.position_.y, 4);
        store(bytes, 8, state.z_, 4);
        store(bytes, 20, state.velocity_.x, 4);
        store(bytes, 24, state.velocity_.y, 4);
        store(bytes, 28, state.vertical_velocity_, 4);
        store(bytes, 32, state.acceleration_.x, 4);
        store(bytes, 36, state.acceleration_.y, 4);
        store(bytes, 40, state.previous_position_.x, 4);
        store(bytes, 44, state.previous_position_.y, 4);
        store(bytes, 48, state.local_position_.x, 4);
        store(bytes, 52, state.local_position_.y, 4);
        store(bytes, 76, state.surface_height_, 4);
        store(bytes, 80, state.falling_height_, 4);
        store(bytes, 84, state.falling_velocity_, 4);
        store(bytes, 88, state.transition_kind_, 4);
        store(bytes, 92, state.rule_timer_, 4);
        store(bytes, 140, state.active_, 2);
        store(bytes, 144, std::uint16_t(state.motion_type_), 2);
        store(bytes, 146, std::uint16_t(state.motion_mode_), 2);
        store(bytes, 148, state.contact_retry_, 2);
        store(bytes, 152, state.layer_, 2);
        constexpr std::array<std::size_t, 79> offsets = {12,  13,  14,  15,  16,  17,  18,  19,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,
                                                         75,  96,  97,  98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
                                                         122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 142, 143, 150, 151, 154, 155, 156};
        for (std::size_t i = 0; i < offsets.size(); ++i)
            bytes[offsets[i]] = state.compatibility_bytes_[i];
        return bytes;
    }
    static void decode(physics::FlipperState& state, std::span<const std::uint8_t> bytes) {
        if (bytes.size() != 148)
            throw std::invalid_argument("Invalid FlipperState snapshot length");
        state.pivot_x = word(bytes, 0);
        state.pivot_y = word(bytes, 4);
        state.angle = word(bytes, 12);
        state.previous_angle = word(bytes, 20);
        state.base_contact_radius_squared = word(bytes, 28);
        state.tip_contact_radius_squared = word(bytes, 32);
        state.base_radius = word(bytes, 36);
        state.tip_radius = word(bytes, 40);
        state.world_x_min = word(bytes, 44);
        state.world_x_max = word(bytes, 48);
        state.world_y_min = word(bytes, 52);
        state.world_y_max = word(bytes, 56);
        state.local_x_max = word(bytes, 68);
        state.local_x_min = word(bytes, 72);
        state.local_y_max = word(bytes, 76);
        state.local_y_min = word(bytes, 80);
        state.length = word(bytes, 84);
        state.slope = word(bytes, 88);
        state.intercept = word(bytes, 92);
        state.rest_angle = word(bytes, 96);
        state.powered_angle = word(bytes, 100);
        state.lower_normal = word(bytes, 108);
        state.upper_normal = word(bytes, 112);
        state.normal_offset_x = word(bytes, 116);
        state.normal_offset_y = word(bytes, 120);
        state.direction = word(bytes, 104);
        state.drive = word(bytes, 124);
        state.return_latch = word(bytes, 140);
        state.powered_latch = word(bytes, 144);
        constexpr std::array<std::size_t, 32> offsets = {8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27, 60, 61, 62, 63, 64, 65, 66, 67, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139};
        for (std::size_t i = 0; i < offsets.size(); ++i)
            state.compatibility_bytes_[i] = bytes[offsets[i]];
    }
    static Bytes encode(const physics::FlipperState& state) {
        Bytes bytes(148);
        store(bytes, 0, state.pivot_x, 4);
        store(bytes, 4, state.pivot_y, 4);
        store(bytes, 12, state.angle, 4);
        store(bytes, 20, state.previous_angle, 4);
        store(bytes, 28, state.base_contact_radius_squared, 4);
        store(bytes, 32, state.tip_contact_radius_squared, 4);
        store(bytes, 36, state.base_radius, 4);
        store(bytes, 40, state.tip_radius, 4);
        store(bytes, 44, state.world_x_min, 4);
        store(bytes, 48, state.world_x_max, 4);
        store(bytes, 52, state.world_y_min, 4);
        store(bytes, 56, state.world_y_max, 4);
        store(bytes, 68, state.local_x_max, 4);
        store(bytes, 72, state.local_x_min, 4);
        store(bytes, 76, state.local_y_max, 4);
        store(bytes, 80, state.local_y_min, 4);
        store(bytes, 84, state.length, 4);
        store(bytes, 88, state.slope, 4);
        store(bytes, 92, state.intercept, 4);
        store(bytes, 96, state.rest_angle, 4);
        store(bytes, 100, state.powered_angle, 4);
        store(bytes, 108, state.lower_normal, 4);
        store(bytes, 112, state.upper_normal, 4);
        store(bytes, 116, state.normal_offset_x, 4);
        store(bytes, 120, state.normal_offset_y, 4);
        store(bytes, 104, state.direction, 4);
        store(bytes, 124, state.drive, 4);
        store(bytes, 140, state.return_latch, 4);
        store(bytes, 144, state.powered_latch, 4);
        constexpr std::array<std::size_t, 32> offsets = {8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27, 60, 61, 62, 63, 64, 65, 66, 67, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139};
        for (std::size_t i = 0; i < offsets.size(); ++i)
            bytes[offsets[i]] = state.compatibility_bytes_[i];
        return bytes;
    }
    static void decode(physics::SpinnerState& state, std::span<const std::uint8_t> bytes) {
        if (bytes.size() != 24)
            throw std::invalid_argument("Invalid SpinnerState snapshot length");
        state.velocity = word(bytes, 12);
        state.phase = word(bytes, 16);
        constexpr std::array<std::size_t, 16> offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 20, 21, 22, 23};
        for (std::size_t i = 0; i < offsets.size(); ++i)
            state.compatibility_bytes_[i] = bytes[offsets[i]];
    }
    static Bytes encode(const physics::SpinnerState& state) {
        Bytes bytes(24);
        store(bytes, 12, state.velocity, 4);
        store(bytes, 16, state.phase, 4);
        constexpr std::array<std::size_t, 16> offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 20, 21, 22, 23};
        for (std::size_t i = 0; i < offsets.size(); ++i)
            bytes[offsets[i]] = state.compatibility_bytes_[i];
        return bytes;
    }
};
enum class FlipperField : std::size_t {
    pivot_x = 0,
    pivot_y = 4,
    angle = 12,
    previous_angle = 20,
    base_contact_radius_squared = 28,
    tip_contact_radius_squared = 32,
    base_radius = 36,
    tip_radius = 40,
    world_x_min = 44,
    world_x_max = 48,
    world_y_min = 52,
    world_y_max = 56,
    local_x_max = 68,
    local_x_min = 72,
    local_y_max = 76,
    local_y_min = 80,
    length = 84,
    slope = 88,
    intercept = 92,
    rest_angle = 96,
    powered_angle = 100,
    lower_normal = 108,
    upper_normal = 112,
    normal_offset_x = 116,
    normal_offset_y = 120,
    direction = 104,
    drive = 124,
    return_latch = 140,
    powered_latch = 144
};
template <class State> inline auto& member(State& state, FlipperField field) {
    switch (field) {
        case FlipperField::pivot_x:
            return state.pivot_x;
        case FlipperField::pivot_y:
            return state.pivot_y;
        case FlipperField::angle:
            return state.angle;
        case FlipperField::previous_angle:
            return state.previous_angle;
        case FlipperField::base_contact_radius_squared:
            return state.base_contact_radius_squared;
        case FlipperField::tip_contact_radius_squared:
            return state.tip_contact_radius_squared;
        case FlipperField::base_radius:
            return state.base_radius;
        case FlipperField::tip_radius:
            return state.tip_radius;
        case FlipperField::world_x_min:
            return state.world_x_min;
        case FlipperField::world_x_max:
            return state.world_x_max;
        case FlipperField::world_y_min:
            return state.world_y_min;
        case FlipperField::world_y_max:
            return state.world_y_max;
        case FlipperField::local_x_max:
            return state.local_x_max;
        case FlipperField::local_x_min:
            return state.local_x_min;
        case FlipperField::local_y_max:
            return state.local_y_max;
        case FlipperField::local_y_min:
            return state.local_y_min;
        case FlipperField::length:
            return state.length;
        case FlipperField::slope:
            return state.slope;
        case FlipperField::intercept:
            return state.intercept;
        case FlipperField::rest_angle:
            return state.rest_angle;
        case FlipperField::powered_angle:
            return state.powered_angle;
        case FlipperField::lower_normal:
            return state.lower_normal;
        case FlipperField::upper_normal:
            return state.upper_normal;
        case FlipperField::normal_offset_x:
            return state.normal_offset_x;
        case FlipperField::normal_offset_y:
            return state.normal_offset_y;
        case FlipperField::direction:
            return state.direction;
        case FlipperField::drive:
            return state.drive;
        case FlipperField::return_latch:
            return state.return_latch;
        case FlipperField::powered_latch:
            return state.powered_latch;
    }
    throw std::out_of_range("Unknown physics snapshot field");
}
inline std::int32_t get(const physics::FlipperState& state, FlipperField field) {
    return member(state, field);
}
inline void set(physics::FlipperState& state, FlipperField field, std::int32_t value) {
    member(state, field) = value;
}
enum class SpinnerField : std::size_t {
    velocity = 12,
    phase = 16
};
template <class State> inline auto& member(State& state, SpinnerField field) {
    switch (field) {
        case SpinnerField::velocity:
            return state.velocity;
        case SpinnerField::phase:
            return state.phase;
    }
    throw std::out_of_range("Unknown physics snapshot field");
}
inline std::int32_t get(const physics::SpinnerState& state, SpinnerField field) {
    return member(state, field);
}
inline void set(physics::SpinnerState& state, SpinnerField field, std::int32_t value) {
    member(state, field) = value;
}
template <class State>
    requires requires(const State& state) { PhysicsSnapshot::encode(state); }
inline Bytes encode(const State& state) {
    return PhysicsSnapshot::encode(state);
}
inline physics::BallState read_ball(std::span<const std::uint8_t> bytes) {
    return PhysicsSnapshot::read<physics::BallState>(bytes);
}
inline physics::FlipperState read_flipper(std::span<const std::uint8_t> bytes) {
    return PhysicsSnapshot::read<physics::FlipperState>(bytes);
}
inline physics::SpinnerState read_spinner(std::span<const std::uint8_t> bytes) {
    return PhysicsSnapshot::read<physics::SpinnerState>(bytes);
}
inline physics::MechanismState read_mechanisms(std::span<const std::uint8_t> bytes) {
    return PhysicsSnapshot::read<physics::MechanismState>(bytes);
}
} // namespace web::compatibility
