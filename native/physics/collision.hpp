#pragma once
#include "ball.hpp"
#include "nudge.hpp"
#include <utility>
#include <memory>

namespace web::physics {
class CollisionGeometry {
public:
    explicit CollisionGeometry(std::array<TileMap, 3> layers);
    static CollisionGeometry load(const std::filesystem::path& assets);
    std::uint8_t sample(std::uint16_t layer, int x, int y) const;

private:
    std::shared_ptr<const std::array<TileMap, 3>> layers_;
};

class AngleLookup {
public:
    // Original 0x4148c4: zero points along +Y; returned angle is UNSIGNED 16-bit.
    // Throws for INT32_MIN and normalization that would overrun the original table.
    static std::uint16_t direction(Vector2 velocity);
    static std::span<const std::int16_t> table();
    static std::span<const std::int16_t> sine_table();
    static std::int32_t sine(std::uint16_t angle);
    static std::int32_t cosine(std::uint16_t angle) {
        return sine(std::uint16_t(angle + physics::angle::quarter_turn));
    }
};
using DirectionLookup = std::uint16_t (*)(Vector2);
using RepairRecovery = void (*)(const CollisionGeometry&, BallState&);
struct PhysicsPolicy {
    DirectionLookup direction = AngleLookup::direction;
    RepairRecovery repair_cycle = nullptr;
    bool (*flipper_eligible)(const BallState&) = nullptr;
    unsigned (*nudge_step)(NudgeState&, std::uint16_t, const std::function<void()>&) = update_nudge;
};

class CollisionResponse {
public:
    using Coefficients = std::array<std::int16_t, 256>;
    CollisionResponse(Coefficients low, Coefficients high) :
        data_(std::make_shared<const Data>(Data{low, high})) {}
    static CollisionResponse load(const std::filesystem::path& assets);
    // Original 0x4018a0. Flags 0x100 OR 0x200 select BVEC0_50.
    void reflect(BallState& ball, std::uint16_t contact, Vector2 nudge = {}, DirectionLookup direction = AngleLookup::direction) const;

private:
    struct Data {
        Coefficients low, high;
    };
    std::shared_ptr<const Data> data_;
};

class CollisionClassifier {
public:
    explicit CollisionClassifier(CollisionGeometry geometry, RepairRecovery recovery = nullptr) :
        geometry_(std::move(geometry)),
        recovery_(recovery) {}
    // Original 0x401fd8; repairs position in place, returns -1 for no response.
    int classify(BallState& ball) const;

private:
    CollisionGeometry geometry_;
    RepairRecovery recovery_;
};
} // namespace web::physics
