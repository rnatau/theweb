#include "idle.hpp"

namespace web::rules {
void IdleRules::step(physics::SimulationState& simulation, TurnState& rules, SharedGameState& session, LampState& state) const {
    namespace F = web::rules::turn_field;
    LampRules lamps;
    if (rules.lamp_animation)
        lamps.step_animation(rules, session);
    else
        lamps.compose_table(simulation, rules, session, state);
    const auto mode = rules.lamp_mode;
    if ((!rules.lamp_animation || mode != 0) && mode != 99)
        lamps.begin_animation(rules, session, 0);
    simulation.random.next();
}
} // namespace web::rules
