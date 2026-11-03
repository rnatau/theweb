#pragma once
#include "lamps.hpp"

namespace web::rules {
// Original phase zero at 0x411d25..0x411d6c. DMD cadence remains independent.
class IdleRules {
public:
    void step(physics::SimulationState&, TurnState&, SharedGameState&, LampState&) const;
};
} // namespace web::rules
