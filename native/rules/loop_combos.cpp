#include "loop_combos.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr int mania_active = 2, short_flash = rule_time::quarter_second, award_flash = rule_time::second;
constexpr int combo_cap = 5, combo_units = 500000, combo_window = (5 * rule_time::second);
constexpr int loop_window = (7 * rule_time::half_second), gate_window = rule_time::half_second, completion_window = (2 * rule_time::second), lane_window = 4 * rule_time::second;
constexpr auto loop_first_gate = F::loop_first_gate_timer, loop_last_gate = F::loop_last_gate_timer, loop_remaining = F::loop_remaining;
constexpr auto combo_remaining = S::combo_remaining, mania_remaining = S::combo_mania_timer;
} // namespace
Player& LoopComboRules::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void LoopComboRules::animation(unsigned id, std::uint32_t flags) {
    if (effects_)
        effects_->loop_combo_animation(id, flags);
}
void LoopComboRules::page(LoopComboPage kind, int count, ScoreValue amount) {
    if (effects_)
        effects_->loop_combo_page(kind, count, amount);
}
void LoopComboRules::announce() {
    scoring_.announce();
}
void LoopComboRules::timer(web::rules::TurnMember field) {
    if (const auto before = (rules_.*field))
        (rules_.*field) = std::max(0, sub(before, rules_.elapsed));
}
void LoopComboRules::timer(web::rules::SessionMember field) {
    if (const auto before = (session_.*field))
        (session_.*field) = std::max(0, sub(before, rules_.elapsed));
}
void LoopComboRules::loop() {
    auto& p = player();
    rules_.super_skillshot = 1;
    if (!rules_.mission_time && !rules_.drain_override && rules_.single_ball_award_mode != mania_active)
        animation(9);
    const auto count = add(session_.loop_count, 1);
    session_.loop_count = count;
    scoring_.sound({184, 0, sub(112, mul(std::min(count, 12), 112) / 12), 18});
    p.bonus.loops = add(p.bonus.loops, 1);
    if (count > p.loops.best)
        p.loops.best = count;
    bool milestone = false;
    if (count >= p.loops.extra_ball_threshold) {
        milestone = !rules_.extra_ball_diverter;
        if (milestone)
            animation(33);
        if (milestone)
            scoring_.sound({35, 64, 0, 1});
        rules_.extra_ball_diverter = std::max((11 * rule_time::half_second), sub((15 * rule_time::second), bits(std::uint32_t(p.loops.extra_ball_threshold) << 16)));
        rules_.diverter_warning = 1;
    }
    constexpr std::array<std::uint32_t, 4> awards = {500000, 1000000, 2000000, 4000000};
    if (count == 3 && !p.bonus.hold) {
        p.bonus.hold = 1;
        milestone = true;
        animation(3);
        scoring_.sound({61, 64, 0, 1});
    } else if (count == 4 && !p.bumpers.hold) {
        p.bumpers.hold = 1;
        milestone = true;
        animation(4);
        scoring_.sound({6, 64, 0, 1});
    } else if ((count < 1 || count > int(awards.size())) && !milestone)
        announce();
    scoring_.add(count >= 1 && count <= int(awards.size()) ? awards[unsigned(count - 1)] : 5000000);
    if (!milestone)
        page(LoopComboPage::loops, count);
}
void LoopComboRules::combo() {
    auto& p = player();
    LampRules{}.queue(rules_, session_, TableLamp::combo, short_flash);
    const auto count = add(session_.combo_count, 1), factor = std::min(count, combo_cap);
    session_.combo_count = count;
    session_.combo_remaining = combo_window;
    if (p.combos.state != mania_active) {
        scoring_.add(std::uint32_t(mul(factor, combo_units)));
        const auto remaining = std::max(0, sub(p.combos.remaining, factor));
        p.combos.remaining = remaining;
        if (remaining < 10)
            page(remaining ? LoopComboPage::mania_remaining : LoopComboPage::mania_ready, remaining);
        p.bonus.combos = add(p.bonus.combos, 1);
        if (count > p.combos.best)
            p.combos.best = count;
    } else if (session_.combo_mania_timer) {
        const auto amount = std::uint32_t(mul(mul(add(mul(p.combos.rounds, 5), 20), factor), 100000));
        scoring_.add(amount);
        display_.value = amount % ScoreValue::radix;
        page(LoopComboPage::mania_score, 0, {display_.value, (amount / ScoreValue::radix) % 10});
        if (count != 1)
            p.bonus.combos = add(p.bonus.combos, 1);
        if (sub(count, 1) > p.combos.best)
            p.combos.best = sub(count, 1);
    }
    if (factor == 1) {
        constexpr std::array<unsigned, 8> animations = {88, 89, 90, 88, 89, 90, 94, 95};
        animation(animations[(rules_.frame_phase() >> 4) & 7]);
        scoring_.sound({session_.combo_mania_timer ? 93 : 7, session_.combo_mania_timer ? -64 : 64, 0, 1});
    } else {
        if (factor < 2 || factor > 4 || session_.combo_mania_timer)
            announce();
        else
            scoring_.sound({factor + 6, 64, 0, 1});
        animation(factor == 2 ? 91 : factor == 3 ? 92 : 93);
    }
    if (!p.combos.remaining) {
        LampRules{}.queue(rules_, session_, TableLamp::quick_shot, award_flash);
        p.combos.state = 1;
    }
}
void LoopComboRules::step_loops() {
    rules_.loop_completed = 0;
    if (rules_.switch_edge(TableSwitch::loop_entry))
        rules_.loop_first_gate_timer = gate_window;
    if (rules_.loop_first_gate_timer) {
        timer(loop_first_gate);
        if (rules_.switch_edge(TableSwitch::loop_middle)) {
            rules_.loop_first_gate_timer = 0;
            rules_.loop_last_gate_timer = completion_window;
            if (!rules_.auto_kick_enabled && !rules_.drain_override && !rules_.mission_time && rules_.single_ball_award_mode != mania_active)
                animation(8, 0);
        }
    }
    if (rules_.loop_last_gate_timer) {
        timer(loop_last_gate);
        if (rules_.switch_edge(TableSwitch::loop_gate_b)) {
            rules_.loop_last_gate_timer = 0;
            rules_.loop_remaining = loop_window;
            loop();
            if (!rules_.drain_override && !rules_.launch_timer)
                session_.web_ramp_combo_timer = lane_window;
            rules_.loop_completed = 1;
        }
    }
    if (rules_.loop_remaining) {
        timer(loop_remaining);
        if (!rules_.loop_remaining)
            session_.loop_count = 0;
    }
    if (rules_.extra_ball_diverter) {
        timer(F::extra_ball_diverter);
        if (rules_.extra_ball_diverter < lane_window && rules_.diverter_warning == 1) {
            rules_.diverter_warning = 0;
            const bool alternate = ((rules_.frame_phase() >> 3) & 1) != 0;
            scoring_.sound({alternate ? 87 : 47, alternate ? 0 : 64, 0, 1});
        }
    }
}
void LoopComboRules::step_combos() {
    constexpr auto left_entry = S::left_combo_entry_timer, right_entry = S::right_combo_entry_timer;
    if (!rules_.drain_override) {
        if (rules_.switch_edge(41))
            session_.left_combo_entry_timer = lane_window;
        if (rules_.switch_edge(42))
            session_.right_combo_entry_timer = lane_window;
    }
    const auto consume = [&](web::rules::SessionMember field, bool contact) {
        if (!(session_.*field))
            return;
        timer(field);
        if (contact) {
            (session_.*field) = 0;
            combo();
        }
    };
    consume(left_entry, rules_.left_loop_completed != 0);
    consume(S::right_loop_lamp_timer, rules_.switch_edge(TableSwitch::ramp_a));
    consume(S::web_ramp_combo_timer, rules_.switch_edge(TableSwitch::web_ramp));
    consume(S::left_loop_lamp_timer, rules_.switch_edge(TableSwitch::web_ramp) || rules_.loop_completed);
    consume(right_entry, session_.right_loop_completed || rules_.switch_edge(TableSwitch::ramp_a));
    if (session_.combo_remaining) {
        timer(combo_remaining);
        if (!session_.combo_remaining)
            session_.combo_count = 0;
    }
    if (session_.combo_mania_timer) {
        timer(mania_remaining);
        if (!session_.combo_mania_timer)
            player().combos.state = 0;
    }
}
} // namespace web::rules
