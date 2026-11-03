#pragma once
#include "lock_awards.hpp"
#include "ramp_awards.hpp"
#include "video_mode.hpp"
#include "power_levels.hpp"

namespace web::rules {
enum class ExplosionPage : unsigned {
    superjets = 49,
    quick_shot = 50,
    video_ready = 51,
    super_rescue = 52,
    outlanes = 53,
    saver = 75
};
struct ExplosionState {
    std::int32_t replay_delay = 0; // Original 0x42c4f8; announcer playback remains separate.
};
class ExplosionEffects {
public:
    virtual ~ExplosionEffects() = default;
    virtual void explosion_page(ExplosionPage page, unsigned ticks, int priority) = 0;
};
// Original 32-way Explosion award selection. Helpers retain their existing
// score/lamp/video/lock owners; the simulation owns the shared Watcom RNG.
class ExplosionRules {
public:
    ExplosionRules(
        TurnState& rules,
        SharedGameState& session,
        physics::SimulationState& simulation,
        ScoringRules& scoring,
        PowerLevelRules& power,
        RampAwardRules& ramps,
        ExtraBallRules& extra,
        VideoModeRules& video,
        LockAwards& locks,
        ExplosionState& state,
        SingleBallAwardEffects* awards = nullptr,
        ExplosionEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        simulation_(simulation),
        scoring_(scoring),
        power_(power),
        ramps_(ramps),
        extra_(extra),
        video_(video),
        locks_(locks),
        state_(state),
        awards_(awards),
        effects_(effects) {}
    void step();                                               // 0x40d9f8, wait then retry unavailable random awards.
    void timing_shot(std::uint32_t edges, std::uint32_t held); // 0x40eb89..0x40ec0b.
private:
    TurnState& rules_;
    SharedGameState& session_;
    physics::SimulationState& simulation_;
    ScoringRules& scoring_;
    PowerLevelRules& power_;
    RampAwardRules& ramps_;
    ExtraBallRules& extra_;
    VideoModeRules& video_;
    LockAwards& locks_;
    ExplosionState& state_;
    SingleBallAwardEffects* awards_;
    ExplosionEffects* effects_;
    Player& player();
    bool apply(unsigned choice);
    void animation(unsigned id, std::uint32_t flags);
    void page(ExplosionPage page);
};
} // namespace web::rules
