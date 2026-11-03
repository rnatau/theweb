#include "ramp_awards.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;

using namespace physics::integer;
namespace {
constexpr int award_flash = rule_time::second, entry_duration = 2 * rule_time::second;
constexpr std::uint32_t entry_score = 1000, arena_score = 100000;
constexpr int base_weight = 1, single_inlane_weight = 2, double_inlane_weight = 4;
constexpr int first_lock = 1, lock_increment = 5, first_extra_ball = 20, extra_ball_increment = 50;
constexpr std::array<web::rules::TurnMember, 3> ramp_award_flags = {F::magnet_combo_timer, F::station_ramp_ready, F::station_super_stage};
} // namespace
Player& RampAwardRules::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void RampAwardRules::animation(unsigned id, std::uint32_t flags) {
    if (effects_)
        effects_->ramp_animation(id, flags);
}
void RampAwardRules::arena_entry() {
    if (!rules_.mission_time && !rules_.drain_override && rules_.single_ball_award_mode != 2) {
        if (rules_.ramp_a_entry_timer)
            animation(86, 1);
        else
            animation(rules_.inlane_a_timer && rules_.inlane_b_timer ? 84 : 83, 0);
    }
    scoring_.add(entry_score);
}
void RampAwardRules::web_entry() {
    if (rules_.web_ramp_entry_timer && !rules_.drain_override && !rules_.mission_time)
        animation(72, 0);
    else {
        const bool special =
            player().station.state == mode_state::ready || std::any_of(ramp_award_flags.begin(), ramp_award_flags.end(), [&](web::rules::TurnMember field) { return (rules_.*field) != 0; });
        if (special)
            animation(16, 1);
        else if (!rules_.drain_override && !rules_.mission_time)
            animation(71, 0);
    }
    scoring_.add(entry_score);
}
bool RampAwardRules::light_lock() {
    auto& p = player();
    if (p.lock.state)
        return false;
    LampRules{}.queue(rules_, session_, TableLamp::lock_ready, award_flash);
    const bool display = !rules_.drain_override;
    if (display && effects_)
        effects_->lock_ready();
    if (display)
        scoring_.sound({121, -64, 0, 1});
    p.lock.state = mode_state::ready;
    p.lock.progress = 0;
    return display;
}
int RampAwardRules::consume_inlane_weight() {
    int increment = base_weight;
    if (rules_.inlane_a_timer && rules_.inlane_b_timer)
        increment = double_inlane_weight;
    else if (rules_.inlane_a_timer || rules_.inlane_b_timer)
        increment = single_inlane_weight;
    if (increment != base_weight) {
        rules_.inlane_a_timer = 0;
        rules_.inlane_b_timer = 0;
    }
    return increment;
}
bool RampAwardRules::advance_lock_threshold(Player& p, int ramps) {
    bool lock_message = false;
    if (ramps >= p.arena.lock_at) {
        if (!p.lock.state)
            lock_message = light_lock();
        const auto threshold = p.arena.lock_at;
        p.arena.lock_at = threshold == first_lock ? lock_increment : add(threshold, lock_increment);
    }
    return lock_message;
}
void RampAwardRules::advance_extra_ball_threshold(Player& p, int ramps) {
    if (ramps >= p.arena.extra_ball_at) {
        const auto threshold = p.arena.extra_ball_at;
        p.arena.extra_ball_at = threshold == first_extra_ball ? extra_ball_increment : add(threshold, extra_ball_increment);
        scoring_.sound({126, -64, 0, 1});
        LampRules{}.queue(rules_, session_, TableLamp::extra_ball_lit, award_flash);
        p.awards.extra_balls_lit = add(p.awards.extra_balls_lit, 1);
        animation(33, 1);
    }
}
void RampAwardRules::arena() {
    scoring_.sound({184, 0, 96, 18});
    auto& p = player();
    const bool mission = rules_.mission_time != 0;
    if (!mission && !rules_.drain_extension) {
        rules_.drain_extension = 1;
        LampRules{}.queue(rules_, session_, TableLamp::mission_ready, award_flash);
    }
    if (!mission && !rules_.drain_override && rules_.single_ball_award_mode != 2)
        animation(85, 1);
    const auto increment = consume_inlane_weight();
    const auto ramps = add(p.arena.ramps, increment);
    p.arena.ramps = ramps;
    const bool lock_message = advance_lock_threshold(p, ramps);
    advance_extra_ball_threshold(p, ramps);
    scoring_.add(std::uint32_t(increment) * arena_score);
    display_.value = std::uint32_t(increment);
    // Sample final counters for presentation. A new lock message replaces the
    // earlier arena-status request in the original, so retain that priority.
    if (!mission && !lock_message && effects_)
        effects_->arena_status(ramps, p.arena.lock_at, p.arena.extra_ball_at);
}
void RampAwardRules::step_contacts(const TableMechanismRules& mechanisms) {
    const auto timer = [&](web::rules::TurnMember field) {
        if (const auto value = (rules_.*field))
            (rules_.*field) = std::max(0, sub(value, rules_.elapsed));
    };
    if (rules_.switch_edge(TableSwitch::web_ramp_entry)) {
        web_entry();
        rules_.web_ramp_entry_timer = entry_duration;
    }
    timer(F::web_ramp_entry_timer);
    if (rules_.switch_edge(TableSwitch::web_ramp))
        mechanisms.ramp(rules_, session_);
    if (rules_.switch_edge(TableSwitch::ramp_a_entry)) {
        arena_entry();
        rules_.ramp_a_entry_timer = entry_duration;
    }
    timer(F::ramp_a_entry_timer);
    if (rules_.switch_edge(TableSwitch::ramp_a))
        arena();
}
} // namespace web::rules
