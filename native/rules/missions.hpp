#pragma once
#include "scoring.hpp"
#include "drop_targets.hpp"
#include "extra_balls.hpp"

namespace web::rules {
enum class Mission : unsigned {
    reactor,
    hunt,
    ammo_dump,
    skyscraper,
    shuttle,
    biker,
    count
};
struct MissionState {
    // Only the newly owned globals 0x431760..0x431777. Remaining mission
    // words alias the established SharedGameState, TurnState and player bytes.
    std::int32_t biker_shot = 0, scratch = 0, biker_timer = 0;
    std::int32_t biker_alternate = 0, biker_award = 0, biker_remaining = 0;
    std::int32_t bumper_lamp_phase = 0; // 0x42c510, independent of the six contiguous words.
};
struct MissionStatus {
    std::string name, objective;
    unsigned seconds = 0, progress = 0, target = 0;
    bool active = false;
};
// Original rule-page selectors in 0x41e694. Requests carry DMD ticks, which
// are independent of the simulation's elapsed-time units.
enum class MissionPage : unsigned {
    reactor_intro = 0,
    hunt_intro = 1,
    skyscraper_intro = 6,
    ammo_intro = 29,
    shuttle_intro = 30,
    biker_intro = 31,
    progress = 38,
    cycle_complete = 48,
    total = 61,
    skyscraper_award = 62
};
struct MissionDisplayValues {
    std::int32_t percent = 0, seconds = 0, total = 0, floor = 0, rounds = 0;
};
class MissionEffects {
public:
    virtual ~MissionEffects() = default;
    virtual unsigned mission_animation_active() const = 0;
    virtual void mission_animation(unsigned id, std::uint32_t flags, unsigned repeats = 1) = 0;
    virtual void mission_stop_animation() = 0;
    virtual void mission_clear_page() = 0;
    virtual void mission_page(MissionPage page, unsigned ticks, int priority, bool replace) = 0;
    virtual void mission_progress(int percent, int seconds) = 0;
};
class MissionRules {
public:
    using SelectionFallback = void (*)(Player&);
    MissionRules(
        TurnState& rules,
        SharedGameState& session,
        LampState& lamps,
        ScoringRules& scoring,
        physics::RandomGenerator& random,
        MissionState& state,
        ExtraBallRules* extra_balls = nullptr,
        MissionEffects* effects = nullptr,
        SelectionFallback selection_fallback = nullptr) :
        rules_(rules),
        session_(session),
        lamps_(lamps),
        scoring_(scoring),
        random_(random),
        state_(state),
        extra_balls_(extra_balls),
        effects_(effects),
        selection_fallback_(selection_fallback) {}
    void advance_selection(); // 0x40b2a4, bounded native cycle guard
    void start();             // 0x40d3a4, presentation through MissionEffects; audio isolated
    void step();              // six original mission dispatch arms
    void compose_lamps();
    MissionStatus status() const;
    static std::string name(Mission mission);

private:
    TurnState& rules_;
    SharedGameState& session_;
    LampState& lamps_;
    ScoringRules& scoring_;
    physics::RandomGenerator& random_;
    MissionState& state_;
    ExtraBallRules* extra_balls_;
    MissionEffects* effects_;
    SelectionFallback selection_fallback_;
    Player& player();
    void award(std::int32_t factor);
    void complete();
    void finish();
    void countdown();
    void animation(unsigned id, std::uint32_t flags = 0, unsigned repeats = 1);
    void page(MissionPage kind, unsigned ticks, int priority, bool replace = true);
    void progress(int percent);
    void reactor();
    void hunt();
    void ammo_dump();
    void skyscraper();
    void shuttle();
    void biker();
};
} // namespace web::rules
