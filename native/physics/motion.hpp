#pragma once
#include "collision.hpp"
#include "../effects.hpp"

namespace web::physics {
// Original Watcom rand at 0x4225a5. State belongs to the simulation, not the CRT.
class RandomGenerator {
public:
    explicit RandomGenerator(std::uint32_t seed = 1) :
        state_(seed) {}
    std::uint32_t state() const {
        return state_;
    }
    std::uint32_t next() {
        state_ = state_ * 0x41c64e6d + 0x3039;
        return (state_ >> 16) & 0x7fff;
    }

private:
    std::uint32_t state_;
};
struct DrainContact {};
struct SwitchContact {
    std::int32_t switch_id;
};
struct SurfaceContact {
    std::int32_t transition;
};
struct SpinnerCrossing {
    std::int32_t spinner;
};
struct TiltContact {};
enum class PhysicsEventKind : std::uint32_t {
    drain = 1,
    switch_contact,
    surface_rule,
    sound,
    spinner_crossing,
    tilt
};
struct PhysicsEvent : TypedEffect<PhysicsEventKind, DrainContact, SwitchContact, SurfaceContact, SoundRequest, SpinnerCrossing, TiltContact> {
    static constexpr std::size_t no_ball = 6;
    std::size_t slot;
    template <class T>
    PhysicsEvent(std::size_t source, T value) :
        TypedEffect(value),
        slot(source) {}
};
class PhysicsEventSink {
public:
    virtual ~PhysicsEventSink() = default;
    // Synchronous: rules may change ball state before the caller continues.
    virtual void on_event(const PhysicsEvent& event, BallSlots& balls) = 0;
};
struct PhysicsServices {
    RandomGenerator& random;
    PhysicsEventSink& events;
    DirectionLookup direction = AngleLookup::direction;
};
// A borrowed view: synchronous rule callbacks observe and mutate the same state
// that the motion routine is using. No commit/reload boundary is needed.
struct VectorView {
    std::int32_t& x;
    std::int32_t& y;
    operator Vector2() const {
        return {x, y};
    }
};
struct MotionContext {
    VectorView nudge;
    std::uint32_t& actions;
    std::int32_t& force_timer;
    std::int32_t& control_enabled;
    std::int32_t& control_latched;
    Vector2& control_target;
};
// Standalone motion tools can own these inputs without a complete simulation.
struct MotionState {
    Vector2 nudge;
    std::uint32_t actions = 0;
    std::int32_t force_timer = 0, control_enabled = 0, control_latched = 0;
    Vector2 control_target;
    operator MotionContext() {
        return {
            {nudge.x, nudge.y},
            actions,
            force_timer,
            control_enabled,
            control_latched,
            control_target
        };
    }
};
class BallMotion {
public:
    // Complete 0x402420 including five modes, sixth-ball control and local force.
    void step(BallSlots& balls, std::size_t slot, MotionContext context, PhysicsServices services) const;
};
class SurfaceGeometry {
public:
    SurfaceGeometry(CollisionGeometry collision, std::array<TileMap, 2> heights, std::array<TileMap, 2> switches);
    static SurfaceGeometry load(const std::filesystem::path& assets);
    static SurfaceGeometry load(const std::filesystem::path& assets, CollisionGeometry collision);
    std::uint8_t collision(std::uint16_t layer, int x, int y) const {
        return data_->collision.sample(layer, x, y);
    }
    std::uint8_t height(std::uint16_t layer, int x, int y) const;
    std::uint8_t switches(std::uint16_t layer, int x, int y) const;

private:
    struct Data {
        CollisionGeometry collision;
        std::array<TileMap, 2> heights, switches;
    };
    std::shared_ptr<const Data> data_;
};
class BallSurface {
public:
    explicit BallSurface(SurfaceGeometry geometry) :
        geometry_(std::move(geometry)) {}
    // 0x401b64; external switch/rule/sound effects use the supplied sink.
    void step(BallSlots& balls, std::size_t slot, PhysicsServices services) const;

private:
    SurfaceGeometry geometry_;
};
// Per-ball order from 0x4146e0, excluding surrounding nudge and mechanism ticks.
// The caller owns all state and the original microstep schedule.
class BallPhysics {
public:
    static BallPhysics load(const std::filesystem::path& assets, RepairRecovery recovery = nullptr);
    static BallPhysics load(const std::filesystem::path& assets, const CollisionResponse& response, RepairRecovery recovery);
    BallPhysics(CollisionClassifier classifier, CollisionResponse response, BallSurface surface) :
        classifier_(std::move(classifier)),
        response_(std::move(response)),
        surface_(std::move(surface)) {}
    void step(BallSlots& balls, std::size_t slot, MotionContext context, PhysicsServices services) const;

private:
    BallMotion motion_;
    CollisionClassifier classifier_;
    CollisionResponse response_;
    BallSurface surface_;
};
} // namespace web::physics
