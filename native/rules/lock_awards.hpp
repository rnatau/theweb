#pragma once
#include "single_ball_awards.hpp"
#include "drop_targets.hpp"

namespace web::rules {
enum class LockPage : unsigned {
    no_jackpot = 46,
    choice = 56,
    progress = 74
};
struct LockState {
    std::int32_t announcement_toggle = 0; // Original 0x42c50c, persists across balls.
};
class LockEffects {
public:
    virtual ~LockEffects() = default;
    virtual void lock_page(LockPage page, unsigned ticks, int priority, bool replace) = 0;
    virtual void lock_stop_animations() = 0;
};
// Original lock choice, physical-ball request accounting and jackpot collection.
// The surrounding rules owner supplies contact ordering and video activation.
class LockAwards {
public:
    LockAwards(
        TurnState& rules,
        SharedGameState& session,
        ScoringRules& scoring,
        BonusDisplayState& display,
        LockState& state,
        SingleBallAwardEffects* awards = nullptr,
        LockEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        display_(display),
        state_(state),
        awards_(awards),
        effects_(effects) {}
    void choose(std::uint32_t edges);          // 0x408fcc
    void collect();                            // 0x409468
    void start();                              // 0x409300
    int factor();                              // 0x409170
    void jackpot();                            // 0x409668
    void announce();                           // 0x409578
    void combined();                           // 0x40a187..0x40a281, after the single-ball arms
    void cleanup();                            // 0x40ec9d..0x40ecd6
    bool upper_hole(bool collect_lock = true); // Returns the video branch selected before any lock mutations (0x410675..0x4107a7).
private:
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    BonusDisplayState& display_;
    LockState& state_;
    SingleBallAwardEffects* awards_;
    LockEffects* effects_;
    Player& player();
    void animation(unsigned id, std::uint32_t flags);
    void page(LockPage page, unsigned ticks, int priority, bool replace = true);
    void award(std::uint32_t units, unsigned ticks, int priority, ScoringPage page);
};
} // namespace web::rules
