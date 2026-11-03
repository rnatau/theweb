#include "status.hpp"
#include "../physics/integer.hpp"
#include <utility>

namespace web::rules {
namespace F = web::rules::turn_field;
using namespace physics::integer;
void StatusRules::step(TurnState& rules, std::uint32_t edges, std::uint32_t held, StatusEffects* effects) const {
    constexpr int tap = rule_time::second, advance = (3 * rule_time::second), show = (5 * rule_time::second);
    const auto accumulate_tap = [&](web::rules::TurnMember field) {
        const auto value = add((rules.*field), tap);
        (rules.*field) = value > advance ? show : value;
    };
    // The original cross-connects the edge counters and held counters.
    if (edges & input_action::left_flipper)
        accumulate_tap(F::status_right_hold);
    if (edges & input_action::right_flipper)
        accumulate_tap(F::status_left_hold);
    for (const auto& [field, bit] : {
             std::pair{F::status_left_hold,  1u},
             std::pair{F::status_right_hold, 2u}
    })
        (rules.*field) = held & bit ? add((rules.*field), rules.elapsed) : 0;
    if (rules.status_left_hold > tap || rules.status_right_hold > tap)
        for (unsigned i = 0; i < 43; ++i)
            if (rules.switch_edge(i)) {
                rules.status_left_hold = 0;
                rules.status_right_hold = 0;
            }
    auto selection = rules.status_selection;
    if (rules.status_left_hold > show || rules.status_right_hold > show) {
        if (effects)
            effects->status_page(selection);
        const auto timer = rules.status_timer;
        if (timer > advance || ((edges & input_action::both_flippers) && timer)) {
            selection = add(selection, 1);
            rules.status_timer = 0;
            if (selection > 3) {
                rules.status_left_hold = 0;
                rules.status_right_hold = 0;
                selection = 0;
            }
        }
        rules.status_timer = add(rules.status_timer, rules.elapsed);
    } else {
        selection = 0;
        rules.status_timer = 0;
    }
    rules.status_selection = selection;
}
} // namespace web::rules
