#pragma once
#include "loop_combos.hpp"

namespace web::rules {
class SingleBallAwardEffects {
public:
    virtual ~SingleBallAwardEffects() = default;
    virtual void award_animation(unsigned id, std::uint32_t flags, unsigned repeats = 1) = 0;
    virtual void award_value(ScoreValue amount) = 0;
    virtual void award_page(ScoringPage page, unsigned ticks, int priority) = 0;
    virtual void award_collect(ScoringPage page, unsigned ticks, int priority, std::uint32_t units) = 0;
};
// 0x409708 / the single-ball arms of 0x409a78. Multiball collection remains
// a separate owner; no substitute ball spawning or award formulas are used.
class SingleBallAwards {
public:
    SingleBallAwards(TurnState& rules, SharedGameState& session, ScoringRules& scoring, LoopComboRules& combos, BonusDisplayState& display, SingleBallAwardEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        combos_(combos),
        display_(display),
        effects_(effects) {}
    void refresh_mode(std::uint32_t edges = 0); // 0x40e9f4..0x40eac1
    void start();                               // 0x409708, Super Launcher > Combo Mania > Quick Shot
    void step(int refresh_rate);

private:
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    LoopComboRules& combos_;
    BonusDisplayState& display_;
    SingleBallAwardEffects* effects_;
    Player& player();
    void timer(TurnMember field);
    void animation(unsigned id, std::uint32_t flags = 1, unsigned repeats = 1);
    void collect(std::uint32_t units, unsigned ticks, int priority, ScoringPage page = ScoringPage::award);
    void super_launcher();
    void quick_shot(int refresh_rate);
    void announce();
};
} // namespace web::rules
