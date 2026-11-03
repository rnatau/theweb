#pragma once
#include "../../rules/session_rules.hpp"

namespace web::enhanced {
rules::SessionRules::StartResult start(physics::SimulationState&, rules::TurnState&, rules::SharedGameState&, rules::SessionEffectSink&);
class HoldStart {
public:
    bool operator()(std::uint32_t actions, const rules::TurnState&, int refresh_rate);

private:
    unsigned held_ticks_ = 0;
};
} // namespace web::enhanced
