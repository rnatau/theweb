#include "lock_awards.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr int time_unit = rule_time::second, ready = 1, active = 2;
constexpr unsigned frenzy_music_track = 11;
} // namespace
Player& LockAwards::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void LockAwards::animation(unsigned id, std::uint32_t flags) {
    if (awards_)
        awards_->award_animation(id, flags);
}
void LockAwards::page(LockPage page, unsigned ticks, int priority, bool replace) {
    if (effects_)
        effects_->lock_page(page, ticks, priority, replace);
}
void LockAwards::award(std::uint32_t units, unsigned ticks, int priority, ScoringPage page) {
    if (awards_)
        awards_->award_collect(page, ticks, priority, units);
}
void LockAwards::choose(std::uint32_t edges) {
    rules_.returns_paused = 1;
    display_.continue_seconds = rules_.lock_choice_timer / time_unit;
    page(LockPage::choice, 6, 1);
    auto selection = rules_.lock_choice;
    if (edges & input_action::left_flipper)
        selection = sub(selection, 1);
    if (edges & input_action::right_flipper)
        selection = add(selection, 1);
    selection &= 3;
    rules_.lock_choice = selection;
    rules_.lock_choice_timer = std::max(0, sub(rules_.lock_choice_timer, rules_.elapsed));
    if (!(edges & input_action::launch) && rules_.lock_choice_timer)
        return;
    rules_.returns_paused = 0;
    rules_.lock_choice_timer = 0;
    auto& p = player();
    if (selection != 3 || p.ball.relaunch) {
        p.lock.balls = selection + 2;
        p.lock.progress = add(p.lock.progress, 1);
        scoring_.sound({145, -64, 0, 1});
    } else {
        p.lock.state = mode_state::inactive;
        p.ball.relaunch = 1;
        scoring_.sound({144, -64, 0, 1});
    }
    rules_.lock_choice = selection + 4;
    display_.continue_seconds = -1;
    page(LockPage::choice, 24, 1);
    LampRules{}.queue(rules_, session_, TableLamp::lock_ready, time_unit);
}
void LockAwards::start() {
    auto& p = player();
    const auto required = p.lock.balls, difference = sub(required, rules_.live_balls);
    rules_.super_jackpot = 0;
    session_.mission_centre_lit = 0;
    rules_.mission_time = 0;
    scoring_.stop_music();   // Original 0x4047a0 wrapper, called at 0x409334.
    scoring_.resume_music(); // Original 0x409339, after clearing the single-ball mission.
    DropTargetRules{}.clear_progress(rules_, session_);
    p.lock.state = active;
    p.lock.rounds = add(p.lock.rounds, 1);
    rules_.live_balls = required;
    if (difference > 0)
        rules_.serve_requests = add(rules_.serve_requests, difference);
    rules_.save_timer = bits(std::uint32_t(add(mul(required, 3), 10)) << 15);
    rules_.music_fade_timer = 0;
    scoring_.stop_sounds();
    scoring_.music_track(frenzy_music_track);
    if (!session_.combo_mania_timer) {
        animation(10, 1);
        scoring_.sound({5, 64, 0, 1});
        rules_.combined_lock = 0;
    } else {
        session_.combo_mania_timer = 0;
        p.combos.state = 0;
        animation(7, 1);
        scoring_.sound({12, 64, 0, 1});
        rules_.combined_lock = 1;
        rules_.clear_switch_edge(TableSwitch::sinkhole_b);
    }
}
void LockAwards::collect() {
    LampRules{}.queue(rules_, session_, TableLamp::lock_ready, time_unit);
    auto& p = player();
    if (!p.lock.progress) {
        rules_.lock_choice_timer = 10 * time_unit;
        rules_.lock_choice = 0;
        scoring_.sound({13, 64, 0, 1});
        return;
    }
    const auto progress = add(p.lock.progress, 1);
    p.lock.progress = progress;
    if (progress == p.lock.balls)
        start();
    else {
        if (progress >= 2 && progress <= 4)
            scoring_.sound({144 + progress, -64, 0, 1});
        page(LockPage::progress, 24, 2);
    }
}
int LockAwards::factor() {
    auto& p = player();
    int count = 0;
    for (unsigned target = 0; target < DropTargetRules::count; ++target)
        count += p.targets.at(target) != 0;
    if (count == int(DropTargetRules::count)) {
        if (!rules_.super_jackpot)
            rules_.super_jackpot = 1;
        count = 10;
    }
    return mul(count, mul(add(sub(p.lock.rounds, 1), rules_.live_balls), p.lock.balls));
}
void LockAwards::announce() {
    auto voice = int((rules_.frame_phase() >> 4) & 3);
    if (voice == rules_.announcement_voice)
        voice = add(voice, 1) & 3;
    rules_.announcement_voice = voice;
    if (!rules_.combined_lock)
        animation(voice < 3 ? 11 + unsigned(voice) : 13, 1);
    constexpr int samples[] = {67, 73, 68, 66};
    scoring_.sound({samples[unsigned(voice)], 0, 0, 1});
}
void LockAwards::jackpot() {
    const auto value = factor();
    if (!value) {
        page(LockPage::no_jackpot, 12, -1, false);
        return;
    }
    const auto units = std::uint32_t(mul(value, 100000));
    scoring_.add(units);
    DropTargetRules{}.clear_progress(rules_, session_);
    award(units, 9, -1, ScoringPage::award);
    if (rules_.super_jackpot == 1) {
        auto& p = player();
        p.showdown.spheres = add(p.showdown.spheres, 1);
        animation(125, 0);
        scoring_.hundred_thousands(50);
        rules_.super_jackpot = 2;
    } else
        announce();
}
void LockAwards::combined() {
    auto& p = player();
    if (p.launcher.state == active || p.combos.state == active || p.quick_shot.state == active || !rules_.combined_lock)
        return;
    bool shot = session_.right_loop_completed || rules_.left_loop_completed || rules_.loop_completed;
    for (auto contact : {TableSwitch::ramp_a, TableSwitch::web_ramp, TableSwitch::centre_sinkhole, TableSwitch::upper_sinkhole, TableSwitch::sinkhole_a, TableSwitch::sinkhole_b})
        shot |= rules_.switch_edge(contact);
    if (!shot)
        return;
    const auto value = factor();
    const auto units = std::uint32_t(value ? mul(value, 20000) : mul(rules_.live_balls, 100000));
    // 0x40a231..0x40a247 wraps the multiply by ten before unsigned division.
    scoring_.add((units * 10u) / 10u);
    award(units, 18, 1, ScoringPage::flashing_award);
    state_.announcement_toggle = add(state_.announcement_toggle, 1) & 1;
    if (state_.announcement_toggle) {
        scoring_.announce();
    } else
        announce();
}
void LockAwards::cleanup() {
    auto& p = player();
    if (p.lock.state != active || rules_.live_balls != 1)
        return;
    p.lock.state = mode_state::inactive;
    rules_.combined_lock = 0;
    p.lock.progress = 0;
    if (effects_)
        effects_->lock_stop_animations();
    scoring_.stop_music();
}
bool LockAwards::upper_hole(bool collect_lock) {
    auto& p = player();
    const bool contact = rules_.switch_edge(TableSwitch::sinkhole_b);
    if (contact && !rules_.drain_override && !rules_.mission_time && !p.lock.state && !p.video.ready) {
        if (rules_.inlane_a_timer != -1) {
            rules_.inlane_a_timer = -1;
            LampRules{}.queue(rules_, session_, 48, time_unit);
        } else if (rules_.inlane_b_timer != -1) {
            rules_.inlane_b_timer = -1;
            LampRules{}.queue(rules_, session_, 49, time_unit);
        }
    }
    const bool starts_video = contact && !rules_.mission_time && rules_.single_ball_award_mode != active && p.video.ready == ready && !rules_.drain_override;
    if (collect_lock && ((contact && !starts_video) || (!contact && rules_.video_lock_pending == 2))) {
        if (p.lock.state == ready && !rules_.drain_override && !rules_.super_launcher_timer && !rules_.quick_shot_timer)
            collect();
    }
    return starts_video;
}
} // namespace web::rules
