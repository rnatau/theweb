#include "single_ball_awards.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr int ready = 1, active = 2, time_unit = rule_time::second;
constexpr int initial_quick_value = 25000000, minimum_quick_value = 5000000, quick_decay_per_second = 0x13fd30;
constexpr unsigned launcher_intro = 96, launcher_idle = 97, launcher_hit = 98, launcher_timeout = 99, launcher_countdown = 100, sphere_animation = 125;
constexpr unsigned combo_intro = 6, award_ticks = 9, quick_collect_ticks = 18;
constexpr unsigned launcher_music_track = 2, combo_music_track = 1, quick_music_track = 3;
} // namespace
Player& SingleBallAwards::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void SingleBallAwards::timer(web::rules::TurnMember field) {
    if (const auto before = (rules_.*field))
        (rules_.*field) = std::max(0, sub(before, rules_.elapsed));
}
void SingleBallAwards::animation(unsigned id, std::uint32_t flags, unsigned repeats) {
    if (effects_)
        effects_->award_animation(id, flags, repeats);
}
void SingleBallAwards::collect(std::uint32_t units, unsigned ticks, int priority, ScoringPage page) {
    if (effects_)
        effects_->award_collect(page, ticks, priority, units);
}
void SingleBallAwards::refresh_mode(std::uint32_t edges) {
    auto& p = player();
    rules_.drain_override = rules_.live_balls > 1;
    if (rules_.launch_enabled) {
        if (edges & input_action::left_flipper)
            scoring_.sound({184, 0, 0, 18});
        if (edges & input_action::right_flipper)
            scoring_.sound({183, 0, 0, 18});
    }
    int mode = 0;
    if (!rules_.drain_override && !rules_.showdown_phase) {
        if (p.quick_shot.state == active || p.launcher.state == active || p.combos.state == active)
            mode = active;
        else if ((p.quick_shot.state == ready || p.launcher.state == ready || p.combos.state == ready) && !rules_.mission_time)
            mode = ready;
    }
    rules_.single_ball_award_mode = mode;
}
void SingleBallAwards::start() {
    auto& p = player();
    if (p.launcher.state == ready) {
        scoring_.stop_sounds();
        scoring_.music_track(launcher_music_track);
        scoring_.sound({3, 64, 0, 1});
        rules_.super_launcher_timer = (75 * rule_time::quarter_second);
        rules_.super_launcher_countdown = 10;
        rules_.super_launcher_hits = 0;
        p.launcher.state = active;
        p.launcher.rounds = add(p.launcher.rounds, 1);
        rules_.music_fade_timer = 0;
        animation(launcher_intro);
        animation(launcher_idle, 0);
    } else if (p.combos.state == ready) {
        scoring_.stop_sounds();
        scoring_.music_track(combo_music_track);
        scoring_.sound({11, 64, 0, 1});
        p.combos.remaining = 15;
        p.combos.state = active;
        p.combos.rounds = add(p.combos.rounds, 1);
        if (!p.combos.sphere)
            p.combos.sphere = 1;
        rules_.music_fade_timer = 0;
        animation(combo_intro);
        session_.combo_count = 0;
        session_.combo_mania_timer = (47 * rule_time::second);
    } else if (p.quick_shot.state == ready) {
        scoring_.stop_sounds();
        scoring_.music_track(quick_music_track);
        scoring_.sound({118, -64, 0, 1});
        p.quick_shot.state = active;
        rules_.quick_shot_value = initial_quick_value;
        rules_.quick_shot_timer = 20 * time_unit;
        rules_.quick_shot_warning = 0;
        const auto side = int((rules_.frame_phase() >> 3) & 1);
        rules_.quick_shot_side = side;
        rules_.music_fade_timer = 0;
        rules_.return_toggle = 1 - side;
    }
}
void SingleBallAwards::announce() {
    scoring_.announce();
}
void SingleBallAwards::super_launcher() {
    auto& p = player();
    if (rules_.switch_edge(TableSwitch::web_ramp)) {
        animation(launcher_hit);
        const auto hits = add(rules_.super_launcher_hits, 1);
        rules_.super_launcher_hits = hits;
        const auto value = std::uint32_t(mul(add(mul(p.launcher.rounds, hits), 1), 2500000));
        scoring_.add(value);
        collect(value, award_ticks, -1);
        if (hits == 5) {
            scoring_.stop_music();
            rules_.super_launcher_timer = 0;
            p.launcher.state = mode_state::inactive;
            rules_.award_retrigger_guard = 2 * time_unit;
            animation(launcher_hit, 1, 3);
            scoring_.sound({16, 64, 0, 1});
            p.showdown.spheres = add(p.showdown.spheres, 1);
            animation(sphere_animation, 0);
            scoring_.hundred_thousands(50);
        } else {
            animation(launcher_idle, 0);
            rules_.super_launcher_timer = (67 * rule_time::quarter_second);
            rules_.super_launcher_countdown = 10;
        }
    }
    if (const auto remaining = rules_.super_launcher_timer) {
        auto frame = std::min(23, sub(23, sub(remaining, (11 * rule_time::quarter_second)) / rule_time::half_second));
        if (frame >= 0)
            animation(launcher_countdown + unsigned(frame), 5);
        const auto countdown = rules_.super_launcher_countdown;
        if (countdown >= 0 && countdown <= 10 && sub(remaining, time_unit) < (countdown ? mul(add(countdown, 1), time_unit) : 0)) {
            scoring_.sound({130 + countdown, -64, 0, 1});
            rules_.super_launcher_countdown = sub(countdown, 1);
        }
    }
    if (rules_.super_launcher_timer) {
        timer(F::super_launcher_timer);
        if (!rules_.super_launcher_timer) {
            p.launcher.state = mode_state::inactive;
            scoring_.stop_music();
            scoring_.stop_sounds(-1, -1, 1);
            scoring_.sound({172, 0, 0, 1});
            scoring_.sound({169, 0, 0, 2});
            animation(launcher_timeout);
        }
    }
    rules_.return_toggle = 0;
    p.magnet.hold = 1;
}
void SingleBallAwards::quick_shot(int refresh_rate) {
    if (refresh_rate <= 0)
        throw std::invalid_argument("Nonpositive Quick Shot refresh rate");
    auto& p = player();
    rules_.return_toggle = sub(1, rules_.quick_shot_side);
    auto value = rules_.quick_shot_value;
    if (rules_.quick_shot_timer < 18 * time_unit)
        value = sub(value, quick_decay_per_second / refresh_rate);
    value = std::max(value, minimum_quick_value);
    rules_.quick_shot_value = value;
    const auto amount = std::uint32_t(mul(value, p.quick_shot.count) / 10);
    display_.value = amount;
    if (effects_) {
        effects_->award_value({amount, 0});
        effects_->award_page(ScoringPage::orbit_award, 2, 1);
    }
    timer(F::quick_shot_timer);
    if (!rules_.quick_shot_timer) {
        p.quick_shot.state = 0;
        scoring_.sound({85, 0, 0, 1});
    }
    if (rules_.quick_shot_timer < 12 * time_unit && !rules_.quick_shot_warning) {
        scoring_.sound({48, 64, 0, 1});
        rules_.quick_shot_warning = 1;
    }
    if (rules_.quick_shot_timer < 5 * time_unit && rules_.quick_shot_warning == 1) {
        scoring_.sound({80, 0, 0, 1});
        rules_.quick_shot_warning = 2;
    }
    if ((rules_.quick_shot_side && rules_.left_loop_completed) || (!rules_.quick_shot_side && session_.right_loop_completed)) {
        // The original scores unsigned DIV here, while its presentation above
        // and collection request use signed IDIV on the same wrapped product.
        scoring_.add(std::uint32_t(mul(value, p.quick_shot.count)) / 10);
        collect(amount, quick_collect_ticks, 1, ScoringPage::flashing_award);
        rules_.quick_shot_timer = 0;
        p.quick_shot.state = 0;
        scoring_.stop_music();
        announce();
        p.quick_shot.count = add(p.quick_shot.count, 1);
    }
}
void SingleBallAwards::step(int refresh_rate) {
    timer(F::award_retrigger_guard);
    auto& p = player();
    if (p.launcher.state == active)
        super_launcher();
    else if (p.combos.state == active) {
        if (!session_.combo_count &&
            (rules_.loop_completed || session_.right_loop_completed || rules_.left_loop_completed || rules_.switch_edge(TableSwitch::ramp_a) || rules_.switch_edge(TableSwitch::web_ramp)))
            combos_.combo();
    } else if (p.quick_shot.state == active)
        quick_shot(refresh_rate);
}
} // namespace web::rules
