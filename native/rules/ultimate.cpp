#include "ultimate.hpp"
#include "lock_awards.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr unsigned showdown_music_track = 20;
constexpr auto mode = F::showdown_phase, stage = F::showdown_stage, hits = F::showdown_hits, artwork = F::showdown_artwork;
constexpr auto total = F::showdown_total;
constexpr int intro_interval = 3 * rule_time::half_second;
constexpr int live_ball_limit = 6;
constexpr unsigned start_animation = 73, first_stage_animation = 74, victory_animation = 82;
constexpr std::uint32_t switch_award_units = 100000;
constexpr int shots_per_artwork = 5, required_shots = 21, required_orbits = 4;
} // namespace
Player& UltimateRules::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void UltimateRules::clear_spheres() {
    auto& p = player();
    for (auto* sphere : p.qualification_spheres())
        *sphere = 0;
}
void UltimateRules::animation(unsigned id, std::uint32_t flags) {
    if (awards_)
        awards_->award_animation(id, flags);
}
void UltimateRules::page(UltimatePage value, unsigned ticks, int priority) {
    if (effects_)
        effects_->ultimate_page(value, ticks, priority);
}
void UltimateRules::introduction_pulse(int attenuation, int last_sample) {
    scoring_.sound({170, 0, attenuation, 2});
    scoring_.sound({169, 0, attenuation, 17});
    scoring_.sound({last_sample, 0, attenuation, 18});
}
UltimateDisplayValues UltimateRules::display_values() {
    return {rules_.showdown_intro, player().showdown.spheres, rules_.showdown_reserves, rules_.showdown_total, player().showdown.rounds};
}
void UltimateRules::ready() {
    const auto& p = player();
    for (auto* sphere : p.qualification_spheres())
        if (!*sphere)
            return;
    if (!rules_.showdown_phase && !rules_.mission_time && !rules_.drain_override && rules_.single_ball_award_mode != mode_state::active)
        rules_.showdown_phase = 1;
}
void UltimateRules::start() {
    session_.request_music_track(-1);
    rules_.returns_paused = 1;
    SessionRules{}.clear_lamp_animation(rules_, session_);
    rules_.lamp_animation = 1;
    rules_.lamp_mode = 3;
    rules_.lamp_frame = 0;
    session_.lamp_animation_enabled = 1;
    auto& p = player();
    if (!p.showdown.spheres)
        p.showdown.spheres = 5;
    rules_.showdown_intro = 1;
    rules_.showdown_intro_timer = intro_interval;
    introduction_pulse(64);
    clear_spheres();
}
void UltimateRules::introduction() {
    page(UltimatePage::introduction, 24, 1);
    const auto remaining = std::max(0, sub(rules_.showdown_intro_timer, rules_.elapsed));
    rules_.showdown_intro_timer = remaining;
    if (remaining)
        return;
    const auto previous = rules_.showdown_intro;
    rules_.showdown_intro = add(previous, 1);
    rules_.showdown_intro_timer = intro_interval;
    if (previous >= 1 && previous <= 6)
        introduction_pulse(72 - 8 * previous);
    else if (previous == 7)
        introduction_pulse(0, 168);
    if (previous != 9)
        return;
    SessionRules{}.clear_lamp_animation(rules_, session_);
    if (effects_)
        effects_->ultimate_clear_page();
    rules_.music_fade_timer = 0;
    scoring_.stop_sounds();
    scoring_.music_track(showdown_music_track);
    rules_.showdown_intro = 0;
    animation(start_animation, 1);
    animation(first_stage_animation, 4);
    scoring_.sound({53, 64, 0, 1});
    rules_.live_balls = live_ball_limit;
    rules_.showdown_phase = 2;
    rules_.serve_requests = add(rules_.serve_requests, 5);
    rules_.save_timer = (45 * rule_time::second);
    // 0x431b80 is an original reset-only word with no confirmed gameplay role.
    for (auto field : {stage, hits, artwork, F::showdown_acceleration, F::showdown_finish_timer, &TurnState::reserved_6, total, F::returns_paused})
        (rules_.*field) = 0;
    auto& p = player();
    p.magnet.hold = 0;
    rules_.showdown_reserves = p.showdown.spheres;
}
void UltimateRules::step() {
    session_.mission_centre_lit = rules_.live_balls != live_ball_limit && rules_.showdown_reserves != 0;
    if (rules_.switch_edge(TableSwitch::centre_shot) && rules_.showdown_reserves && rules_.live_balls != live_ball_limit) {
        rules_.showdown_reserves = sub(rules_.showdown_reserves, 1);
        page(UltimatePage::reserve, 24, -1);
        rules_.live_balls = add(rules_.live_balls, 1);
        rules_.serve_requests = add(rules_.serve_requests, 1);
        if (rules_.showdown_stage == 1)
            rules_.showdown_stage = 2;
        else if (rules_.showdown_stage == 2)
            rules_.showdown_stage = 1;
    }
    std::uint32_t contacts = 0;
    for (unsigned i = 0; i < 43; ++i)
        contacts += rules_.switch_edge(i) != 0;
    const auto units = contacts * std::uint32_t(player().showdown.spheres);
    scoring_.add(units * switch_award_units);
    rules_.showdown_total = add(rules_.showdown_total, bits(units));
    if (rules_.showdown_stage == 3 && rules_.switch_edge(TableSwitch::centre_sinkhole)) {
        rules_.showdown_stage = 4;
        session_.request_music_track(-1);
        for (auto field : {F::returns_paused, F::save_timer, F::launch_enabled})
            (rules_.*field) = 0;
        simulation_.mechanisms.enabled = 0;
        rules_.tilted = 1;
        rules_.tilt_lock = 1;
        session_.warning_display = -2;
        page(UltimatePage::blackout, 256, 0);
        animation(victory_animation, 1);
        scoring_.sound({169, 0, 0, 2});
        scoring_.sound({169, 0, 0, 17});
    }
    if (rules_.showdown_stage == 1 && session_.right_loop_completed) {
        rules_.showdown_stage = 2;
        rules_.showdown_hits = add(rules_.showdown_hits, 1);
    } else if (rules_.showdown_stage == 2 && rules_.left_loop_completed) {
        rules_.showdown_stage = 1;
        rules_.showdown_hits = add(rules_.showdown_hits, 1);
    }
    if ((rules_.showdown_stage == 1 || rules_.showdown_stage == 2) && rules_.showdown_hits >= required_orbits) {
        rules_.showdown_stage = 3;
        scoring_.sound({42, 64, 0, 1});
    }
    if (!rules_.showdown_stage) {
        unsigned shots = 0;
        for (unsigned i : {17, 18, 20, 31, 8, 7})
            shots += rules_.switch_edge(i) != 0;
        shots += rules_.left_loop_completed != 0;
        shots += session_.right_loop_completed != 0;
        shots += rules_.loop_completed != 0;
        rules_.showdown_hits = add(rules_.showdown_hits, std::int32_t(shots));
        for (int i = 0; i < 4; ++i)
            if (rules_.showdown_hits > (i + 1) * shots_per_artwork && rules_.showdown_artwork == i) {
                animation(first_stage_animation + unsigned(i), 1);
                animation(first_stage_animation + 1 + unsigned(i), 4);
                rules_.showdown_artwork = i + 1;
            }
        if (rules_.showdown_hits >= required_shots) {
            rules_.showdown_stage = 1;
            rules_.showdown_hits = 0;
            scoring_.sound({41, 64, 0, 1});
        }
    }
    for (int i = 1; i <= 4; ++i)
        if (rules_.showdown_hits == i && rules_.showdown_artwork == i + 3) {
            animation(77 + unsigned(i), 1);
            animation(78 + unsigned(i), 4);
            rules_.showdown_artwork = i + 4;
            if (i < 4) {
                constexpr int samples[] = {78, 75, 76};
                scoring_.sound({samples[unsigned(i - 1)], 0, 0, 1});
            }
        }
    if ((!effects_ || !effects_->ultimate_animation_active()) && std::uint32_t(rules_.showdown_artwork) <= 8)
        animation(first_stage_animation + unsigned(rules_.showdown_artwork), 4);
}
void UltimateRules::cleanup() {
    if (rules_.showdown_phase <= 1 || rules_.live_balls != 1)
        return;
    session_.mission_centre_lit = 0;
    rules_.showdown_phase = 0;
    clear_spheres();
    if (effects_)
        effects_->ultimate_stop_animations();
    page(UltimatePage::total, 36, 1);
    rules_.super_rescue = 0;
    scoring_.sound({88, 0, 0, 1});
    session_.request_music_track(-1);
}
void UltimateRules::finish() {
    constexpr auto timer = F::showdown_finish_timer, sound_timer = F::showdown_sound_timer;
    const auto decrement = [&](web::rules::TurnMember field) { ((rules_.*field) = std::max(0, sub((rules_.*field), rules_.elapsed))); };
    if (rules_.showdown_stage == 4) {
        if (effects_)
            effects_->ultimate_clear_page();
        decrement(timer);
        if (!rules_.showdown_finish_timer) {
            const auto sound_choice = simulation_.random.next() & 15;
            auto phase = add(rules_.showdown_sound_phase, 1);
            phase = phase > 3 ? 0 : phase;
            rules_.showdown_sound_phase = phase;
            if (std::uint32_t(phase) < 3) {
                constexpr int samples[3][16] = {
                    {63, 64, 66, 67, 68, 69, 70, 85, 71, 72,  73,  74,  77,  78,  79,  80 },
                    {90, 91, 92, 93, 94, 95, 96, 98, 99, 102, 104, 105, 106, 113, 120, 125},
                    {1,  2,  3,  4,  5,  9,  11, 47, 14, 15,  16,  17,  18,  19,  20,  28 }
                };
                constexpr int pans[] = {0, -64, 64};
                scoring_.stop_sounds(-1, -1, 50 + phase);
                scoring_.sound({samples[unsigned(phase)][sound_choice], pans[unsigned(phase)], 0, 50 + phase});
            }
            auto selected = std::int32_t(simulation_.random.next() & 31);
            if (selected == state_.previous_animation)
                selected = (selected + 8) & 31;
            state_.previous_animation = selected;
            constexpr unsigned celebrations[] = {14, 15, 16, 17, 19, 20, 24, 26, 41, 42, 43, 44, 67, 46, 47, 48, 49, 63, 51, 52, 53, 54, 55, 58, 61, 62, 97, 98, 99, 87, 84, 69};
            animation(celebrations[unsigned(selected)], 1);
            rules_.showdown_finish_timer = sub(rule_time::three_quarter_second, rules_.showdown_acceleration);
            rules_.showdown_acceleration = add(rules_.showdown_acceleration, 0x200);
        }
        if (rules_.showdown_acceleration >= (2 * rule_time::second)) {
            if (effects_)
                effects_->ultimate_stop_animations();
            rules_.showdown_finish_timer = (3 * rule_time::second);
            rules_.showdown_stage = 5;
            for (int group : {50, 51, 52})
                scoring_.stop_sounds(-1, -1, group);
            scoring_.sound({52, 64, 0, 1});
            scoring_.sound({52, 64, 0, 18});
            auto& p = player();
            const auto rounds = add(p.showdown.rounds, 1);
            p.showdown.rounds = rounds;
            const auto award = std::uint32_t(rounds) * ScoreValue::radix;
            page(UltimatePage::victory_award, 48, 1);
            if (awards_)
                awards_->award_value({award % ScoreValue::radix, (award / ScoreValue::radix) % 10});
            scoring_.add(std::uint32_t(rules_.showdown_total) * switch_award_units);
            scoring_.add(award);
            rules_.showdown_total = add(mul(rules_.showdown_total, 2), mul(rounds, 1000));
        }
    }
    if (rules_.showdown_stage == 5) {
        decrement(timer);
        if (!rules_.showdown_finish_timer) {
            rules_.showdown_finish_timer = (4 * rule_time::second);
            rules_.showdown_stage = 6;
        }
        rules_.showdown_sound_timer = (2 * rule_time::second);
    } else if (rules_.showdown_stage == 6) {
        if (rules_.showdown_sound_timer) {
            decrement(sound_timer);
            if (!rules_.showdown_sound_timer)
                scoring_.sound({89, 0, 0, 1});
        }
        if (effects_)
            effects_->ultimate_stop_animations();
        page(UltimatePage::total, 48, 1);
        decrement(timer);
        if (!rules_.showdown_finish_timer)
            rules_.showdown_stage = 7;
    } else if (rules_.showdown_stage == 7) {
        if (effects_)
            effects_->ultimate_clear_page();
        clear_spheres();
        session_.mission_centre_lit = 0;
        rules_.showdown_phase = 0;
        if (effects_)
            effects_->ultimate_clear_page();
        for (auto field : {stage, mode, F::tilted, F::tilt_lock})
            (rules_.*field) = 0;
        auto& p = player();
        p.lock.state = mode_state::ready;
        p.lock.balls = live_ball_limit;
        p.lock.progress = live_ball_limit;
        simulation_.mechanisms.enabled = 1;
        locks_.start();
    }
}
void UltimateRules::compose_lamps(LampState& lamps) {
    LampRules composer;
    const auto blink = [&](unsigned lamp, int duration) { composer.blink(rules_, session_, lamps, lamp, duration); };
    blink(10, (16 * rule_time::second));
    if (!(rules_.frame_phase() & 3))
        rules_.showdown_rotating_lamp = add(rules_.showdown_rotating_lamp, 1);
    if (rules_.showdown_rotating_lamp == 5)
        rules_.showdown_rotating_lamp = 0;
    constexpr unsigned sequence[] = {13, 11, 8, 9, 12};
    const auto selected = std::uint32_t(rules_.showdown_rotating_lamp);
    if (selected < 5)
        composer.value(session_, lamps, sequence[selected], 1);
    switch (rules_.showdown_stage) {
        case 0:
            for (unsigned lamp : {55, 52, 38, 30, 47, 44, 35, 32, 46})
                blink(lamp, (16 * rule_time::second));
            break;
        case 1:
        case 2:
            rules_.quick_shot_side = rules_.showdown_stage - 1;
            composer.compose_quick_shot(rules_, session_, lamps);
            break;
        case 3:
            for (unsigned lamp : {55, 54, 53, 34})
                blink(lamp, (8 * rule_time::second));
            break;
    }
}
} // namespace web::rules
