#include "space_station.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr int time_unit = rule_time::second;
constexpr unsigned docked_music_track = 13, frenzy_music_track = 14;
} // namespace
Player& SpaceStationRules::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void SpaceStationRules::animation(unsigned id, std::uint32_t flags) {
    if (awards_)
        awards_->award_animation(id, flags);
}
void SpaceStationRules::jackpot_art(unsigned title, unsigned idle) {
    animation(title, ((rules_.frame_phase() >> 4) & 2) + 1);
    animation(idle, 0);
}
void SpaceStationRules::collect(std::uint32_t units) {
    if (awards_)
        awards_->award_collect(ScoringPage::award, 9, -1, units);
}
void SpaceStationRules::ready() {
    animation(15, 1);
    scoring_.sound({122, -64, 0, 1});
    player().station.state = mode_state::ready;
    LampRules{}.queue(rules_, session_, TableLamp::station_dock, time_unit);
}
void SpaceStationRules::capture(std::size_t slot) {
    auto& p = player();
    if ((p.station.state != mode_state::ready && !rules_.magnet_combo_timer) || p.lock.state == mode_state::active || rules_.single_ball_award_mode == mode_state::active ||
        rules_.showdown_phase == mode_state::active || rules_.award_retrigger_guard)
        return;
    auto& ball = simulation_.balls.at(slot);
    const auto captured = add(p.station.captures, 1);
    p.station.captures = captured;
    rules_.magnet_combo_timer = 0;
    if (captured == 3) {
        rules_.music_fade_timer = 0;
        scoring_.stop_sounds();
        scoring_.music_track(frenzy_music_track);
        session_.mission_centre_lit = 0;
        rules_.mission_time = 0;
        p.station.state = mode_state::active;
        scoring_.sound({30, 64, 0, 1});
        animation(18, 1);
        animation(19, 0);
        rules_.station_ramp_ready = 0;
        rules_.station_super_stage(StationSuperStage::inactive);
        rules_.station_triple_ready = 0;
        rules_.station_centre_stage = 1;
        rules_.live_balls = p.ball.relaunch ? 4 : 3;
        rules_.save_timer = (25 * rule_time::second);
    } else {
        if (captured == 2 && p.station.state == mode_state::ready && !rules_.mission_time) {
            rules_.music_fade_timer = 0;
            scoring_.stop_sounds();
            scoring_.music_track(docked_music_track);
        }
        animation(17, 1);
        if (captured == 1 || captured == 2)
            scoring_.sound({140 + captured, -64, 0, 1});
        rules_.save_timer = (10 * rule_time::second);
    }
    ball.capture_in_station(2 * rule_time::second);
}
void SpaceStationRules::centre_jackpot() {
    LampRules{}.queue(rules_, session_, 9, rule_time::quarter_second);
    const auto stage = rules_.station_centre_stage;
    if (stage < 1 || stage > 3)
        return;
    jackpot_art(20, 20 + unsigned(stage));
    scoring_.sound({22 + stage, 64, 0, 1});
    const auto value = std::uint32_t(player().awards.ramp_value);
    if (stage == 1) {
        if (!rules_.station_ramp_ready)
            rules_.station_centre_stage = 2;
        if (rules_.station_triple_ready == 1) {
            LampRules{}.queue(rules_, session_, 30, time_unit);
            rules_.station_triple_ready = 2;
        }
    } else if (stage == 2) {
        rules_.station_triple_ready = 0;
        rules_.station_centre_stage = add(stage, 1);
    } else {
        rules_.station_centre_stage = 1;
        rules_.station_ramp_ready = 1;
    }
    scoring_.add((value * std::uint32_t(stage)) / 10);
    collect(stage == 3 ? (value / 10) * 3 : (value * std::uint32_t(stage)) / 10);
}
void SpaceStationRules::ramp_jackpot() {
    const auto value = std::uint32_t(player().awards.ramp_value);
    if (rules_.station_super_stage() == StationSuperStage::ramp_collection_window) {
        jackpot_art(26, 27);
        scoring_.sound({28, 64, 0, 1});
        scoring_.sound({28, 64, 0, 17});
        rules_.station_super_stage(StationSuperStage::inactive);
        rules_.station_super_lit = 0;
        auto& p = player();
        if (!p.station.sphere)
            p.station.sphere = 1;
        scoring_.add((value * 10u) / 10u);
        collect(value);
    } else if (rules_.station_ramp_ready) {
        if (rules_.station_super_stage() == StationSuperStage::inactive)
            DropTargetRules{}.clear_progress(rules_, session_);
        jackpot_art(24, 25);
        scoring_.sound({26, 64, 0, 1});
        rules_.station_ramp_ready = 0;
        session_.station_super_holdoff = 0;
        rules_.station_super_stage(StationSuperStage::targets_required);
        for (unsigned target = 0; target < DropTargetRules::count; ++target)
            LampRules{}.queue(rules_, session_, unsigned(TableLamp::drop_target_first) + target, time_unit);
        rules_.station_triple_ready = 1;
        scoring_.add((value * 5u) / 10u);
        collect((value / 10u) * 5u);
    }
}
void SpaceStationRules::triple_jackpot() {
    jackpot_art(28, 29);
    scoring_.sound({29, 64, 0, 1});
    scoring_.sound({171, 0, 0, 18});
    rules_.station_triple_ready = 0;
    const auto value = std::uint32_t(player().awards.ramp_value);
    scoring_.add((value * 3u) / 10u);
    collect((value / 10u) * 3u);
}
void SpaceStationRules::cleanup() {
    auto& p = player();
    if (p.station.state != mode_state::active)
        return;
    const auto active = effects_ ? effects_->station_animation_active() : 0;
    if (rules_.live_balls == 1) {
        rules_.station_ramp_ready = 0;
        rules_.station_super_stage(StationSuperStage::inactive);
        rules_.station_triple_ready = 0;
        p.station.state = mode_state::inactive;
        rules_.station_super_lit = 0;
        p.station.captures = 0;
        DropTargetRules{}.clear_progress(rules_, session_);
        if (effects_)
            effects_->station_finish_animations();
        rules_.super_rescue = 0;
        scoring_.stop_music();
    } else if (!active)
        animation(19, 0);
}
void SpaceStationRules::upper_hole() {
    if ((rules_.mission_time && !session_.mission_kind) || !rules_.switch_edge(TableSwitch::upper_sinkhole))
        return;
    auto& p = player();
    if (p.magnet.ready) {
        p.magnet.ready = 0;
        scoring_.sound({123, -64, 0, 1});
        p.magnet.hold = 1;
        LampRules{}.queue(rules_, session_, 39, time_unit);
        if (!rules_.drain_override)
            animation(5, 0);
    }
    if (!rules_.station_super_lit) {
        for (int target = int(DropTargetRules::count) - 1; target >= 0; --target) {
            auto& progress = p.targets.at(unsigned(target));
            if (!progress) {
                progress = 1;
                LampRules{}.queue(rules_, session_, unsigned(TableLamp::drop_target_first) + unsigned(target), rule_time::quarter_second);
                break;
            }
        }
    } else {
        if (rules_.station_super_stage() > StationSuperStage::targets_required && !session_.station_super_holdoff) {
            rules_.station_super_lit = 0;
            rules_.station_super_stage(StationSuperStage::targets_required);
            scoring_.sound({21, 64, 0, 1});
        }
        DropTargetRules{}.clear_progress(rules_, session_);
    }
}
void SpaceStationRules::timers() {
    if (rules_.station_super_stage() == StationSuperStage::inactive)
        return;
    const auto remaining = std::max(0, sub(session_.station_super_window, rules_.elapsed));
    session_.station_super_window = remaining;
    if (!remaining && rules_.station_super_stage() == StationSuperStage::ramp_collection_window)
        rules_.station_super_stage(StationSuperStage::magnetic_hole_required);
    if (session_.station_super_holdoff)
        session_.station_super_holdoff = std::max(0, sub(session_.station_super_holdoff, rules_.elapsed));
}
} // namespace web::rules
