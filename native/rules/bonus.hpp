#pragma once
#include "scoring.hpp"

namespace web::rules {
struct BonusDisplayState {
    std::uint32_t value = 0;                      // Original 0x447dc8, ten-point display units.
    std::int32_t stage = 0, continue_seconds = 0; // 0x447dd4 / 0x447db4.
};
// Original end-player cleanup 0x405210 and phase four 0x411fa3..0x412490.
// Lamps and presentation requests are composed by the surrounding frame owner.
class BonusRules {
public:
    BonusRules(TurnState& rules, SharedGameState& session, ScoringRules& scoring, BonusDisplayState& display) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        display_(display) {}
    void finish_player();
    void begin();
    void step(std::uint32_t& edges, std::uint32_t held);
    void first_buyin_handler(std::function<void(unsigned, ScoreValue)> handler) {
        first_buyin_ = std::move(handler);
    }

private:
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    BonusDisplayState& display_;
    std::function<void(unsigned, ScoreValue)> first_buyin_;
    Player& current();
    Player& turn();
    std::uint32_t total(std::uint32_t scale) const;
    void award(bool skipped);
    void continue_or_advance(std::uint32_t& edges, std::uint32_t held);
};
} // namespace web::rules
