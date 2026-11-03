#pragma once
#include "lamps.hpp"
#include <functional>

namespace web::rules {
class ScoringRules;
class ExtraBallRules;
// Physical switch controls recovered from 0x40e9e0. Production scoring is an
// injected service; MissionRules owns mission dispatch. Multiball/audio/DMD
// awards remain explicit boundaries.
// Call after rolling the contact latches, before drop targets and lamp drawing,
// only while not tilted, as in the original phase-three rules dispatcher.
class TableMechanismRules {
public:
    // Independent physical-rule fixtures explicitly leave scoring disconnected.
    using Animation = std::function<void(unsigned, std::uint32_t)>;
    using Explosion = std::function<void()>;
    static void restore_kickback(TurnState&, SharedGameState&, ScoringRules*, const Animation&); // 0x407108, shared with Explosion.
    explicit TableMechanismRules(ScoringRules* scoring = nullptr, ExtraBallRules* extra_balls = nullptr, Animation animation = {}, Explosion explosion = {}, std::function<void()> ramp_jackpot = {}) :
        scoring_(scoring),
        extra_balls_(extra_balls),
        animation_(std::move(animation)),
        explosion_(std::move(explosion)),
        ramp_jackpot_(std::move(ramp_jackpot)) {}
    void magnet_timer(const physics::SimulationState&, TurnState&, SharedGameState&) const;                    // 0x40eb51..0x40eb88
    void magnet(physics::SimulationState&, TurnState&, SharedGameState&, bool collect_explosion = true) const; // 0x40f1d3..0x40f383, mission calls isolated
    void kickback(TurnState&, SharedGameState&) const;                                                         // 0x40ef06..0x40f10b
    void outlanes(TurnState&, SharedGameState&) const;                                                         // 0x40f134..0x40f1d2
    void outlane_extra_ball(TurnState&) const;                                                                 // 0x40f10c..0x40f133
    void ramp(TurnState&, SharedGameState&) const;                                                             // physical parts of 0x407898
    void inlanes(TurnState&) const;                                                                            // 0x40f6eb..0x40f79b
    void rollovers(TurnState&, SharedGameState&, std::uint32_t action_edges) const;                            // 0x40f8b8..0x40fb03
    void step(physics::SimulationState&, TurnState&, SharedGameState&, std::uint32_t action_edges) const;
    void step_controls(physics::SimulationState&, TurnState&, SharedGameState&) const;
    void prepare_contacts(physics::SimulationState&, TurnState&, SharedGameState&) const;

private:
    ScoringRules* scoring_;
    ExtraBallRules* extra_balls_;
    Animation animation_;
    Explosion explosion_;
    std::function<void()> ramp_jackpot_;
    void animation(unsigned id, std::uint32_t flags) const;
};
} // namespace web::rules
