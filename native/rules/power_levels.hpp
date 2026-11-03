#pragma once
#include "scoring.hpp"

namespace web::rules {
class PowerLevelEffects {
public:
    virtual ~PowerLevelEffects() = default;
    virtual void power_level(int level, bool animation_allowed) = 0;
};
// Original 0x407194 and its superjet/Quick Shot helpers. Later Quick Shot,
// video-mode and super-launcher activation rules remain separate controllers.
class PowerLevelRules {
public:
    PowerLevelRules(TurnState& rules, SharedGameState& session, ScoringRules& scoring, PowerLevelEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        effects_(effects) {}
    void advance();
    void advance_superjets(); // 0x407ad8
    void ready_quick_shot();  // 0x406b40
    void step_jets();         // inline 0x40f7cc..0x40f88d
private:
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    PowerLevelEffects* effects_;
    Player& player();
};
} // namespace web::rules
