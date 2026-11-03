#pragma once
#include "scoring.hpp"
#include "power_levels.hpp"

namespace web::rules {
// Original inline 0x40f384..0x40f652. Recognizes ordered spinner/rollover
// contacts used by loop objectives; power-level award dispatch stays separate.
class ShotSequenceRules {
public:
    ShotSequenceRules(TurnState& rules, SharedGameState& session, ScoringRules& scoring, PowerLevelRules* power = nullptr) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        power_(power) {}
    void step();

private:
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    PowerLevelRules* power_;
};
} // namespace web::rules
