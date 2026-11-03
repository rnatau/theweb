#pragma once
#include "single_ball_awards.hpp"

namespace web::rules {
class LockAwards;
enum class UltimatePage : unsigned {
    reserve = 40,
    introduction = 44,
    blackout = 58,
    total = 65,
    victory_award = 34
};
struct UltimateDisplayValues {
    int introduction = 0, spheres = 0, reserves = 0, total = 0;
    int rounds = 0;
};
struct UltimateState {
    std::int32_t previous_animation = 0; // Original 0x431758.
};
class UltimateEffects {
public:
    virtual ~UltimateEffects() = default;
    virtual void ultimate_page(UltimatePage, unsigned ticks, int priority) = 0;
    virtual void ultimate_clear_page() = 0;
    virtual void ultimate_stop_animations() = 0;
    virtual unsigned ultimate_animation_active() const = 0;
};
// Original Ultimate Showdown controller. All counters retain their original
// shared rule/session storage; ball requests go through the normal lifecycle.
class UltimateRules {
public:
    UltimateRules(
        TurnState& rules,
        SharedGameState& session,
        physics::SimulationState& simulation,
        ScoringRules& scoring,
        UltimateState& state,
        LockAwards& locks,
        SingleBallAwardEffects* awards = nullptr,
        UltimateEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        simulation_(simulation),
        scoring_(scoring),
        state_(state),
        locks_(locks),
        awards_(awards),
        effects_(effects) {}
    void ready();                   // 0x40eac2..0x40eb26
    void start();                   // 0x40a728
    void introduction();            // 0x40a830
    void step();                    // 0x40abbc
    void cleanup();                 // 0x40ecd7..0x40ed55
    void finish();                  // 0x40a378, after the victory drain
    void compose_lamps(LampState&); // 0x40b0dc
    UltimateDisplayValues display_values();

private:
    TurnState& rules_;
    SharedGameState& session_;
    physics::SimulationState& simulation_;
    ScoringRules& scoring_;
    UltimateState& state_;
    LockAwards& locks_;
    SingleBallAwardEffects* awards_;
    UltimateEffects* effects_;
    Player& player();
    void clear_spheres();
    void animation(unsigned id, std::uint32_t flags);
    void page(UltimatePage, unsigned ticks, int priority);
    void introduction_pulse(int attenuation, int last_sample = 169);
};
} // namespace web::rules
