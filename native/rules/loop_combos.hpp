#pragma once
#include "bonus.hpp"
#include "lamps.hpp"

namespace web::rules {
enum class LoopComboPage {
    loops,
    mania_remaining,
    mania_ready,
    mania_score
};
class LoopComboEffects {
public:
    virtual ~LoopComboEffects() = default;
    virtual void loop_combo_animation(unsigned id, std::uint32_t flags) = 0;
    virtual void loop_combo_page(LoopComboPage page, int count, ScoreValue amount = {}) = 0;
};
// Loop awards 0x406ed0, combos 0x4080c8 and their ordered switch windows.
class LoopComboRules {
public:
    LoopComboRules(TurnState& rules, SharedGameState& session, ScoringRules& scoring, BonusDisplayState& display, LoopComboEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        display_(display),
        effects_(effects) {}
    void loop();
    void combo();
    void step_loops();  // 0x40ed56..0x40ef05, before kickback/shot sequences.
    void step_combos(); // 0x40fb04..0x40fcda, after rollovers.
private:
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    BonusDisplayState& display_;
    LoopComboEffects* effects_;
    Player& player();
    void animation(unsigned id, std::uint32_t flags = 1);
    void page(LoopComboPage kind, int count, ScoreValue amount = {});
    void announce();
    void timer(TurnMember field);
    void timer(SessionMember field);
};
} // namespace web::rules
