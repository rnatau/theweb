#pragma once
#include "bonus.hpp"
#include "table_mechanisms.hpp"

namespace web::rules {
class RampAwardEffects {
public:
    virtual ~RampAwardEffects() = default;
    virtual void ramp_animation(unsigned id, std::uint32_t flags) = 0;
    virtual void arena_status(int ramps, int lock_at, int extra_ball_at) = 0;
    virtual void lock_ready() = 0;
};
// Original entries 0x40759c / 0x407800, arena awards 0x407600, lock lighting
// 0x40925c and their exact contact/timer order at 0x40f653..0x40f6ea.
class RampAwardRules {
public:
    RampAwardRules(TurnState& rules, SharedGameState& session, ScoringRules& scoring, BonusDisplayState& display, RampAwardEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        display_(display),
        effects_(effects) {}
    void arena_entry();
    void web_entry();
    void arena();
    bool light_lock();
    void step_contacts(const TableMechanismRules& mechanisms);

private:
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    BonusDisplayState& display_;
    RampAwardEffects* effects_;
    Player& player();
    void animation(unsigned id, std::uint32_t flags);
    int consume_inlane_weight();
    bool advance_lock_threshold(Player& player, int ramps);
    void advance_extra_ball_threshold(Player& player, int ramps);
};
} // namespace web::rules
