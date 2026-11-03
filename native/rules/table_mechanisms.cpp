#include "table_mechanisms.hpp"
#include "scoring.hpp"
#include "extra_balls.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;
using namespace physics::integer;
namespace {
bool edge(const TurnState& r, unsigned index) {
    return r.switch_edge(index);
}
void timer(TurnState& r, web::rules::TurnMember field) {
    if (const auto before = (r.*field); before != 0)
        (r.*field) = std::max(0, sub(before, r.elapsed));
}
} // namespace
void TableMechanismRules::animation(unsigned id, std::uint32_t flags) const {
    if (animation_)
        animation_(id, flags);
}
void TableMechanismRules::magnet_timer(const physics::SimulationState& simulation, TurnState& r, SharedGameState& s) const {
    if (r.magnet_voice_timer == 0)
        return;
    timer(r, F::magnet_voice_timer);
    auto& p = s.players().at(std::uint32_t(r.current_player));
    if (r.magnet_voice_timer == 0 && simulation.force_timer && p.launcher.state != mode_state::active) {
        // 0x40d7f8 shares the original announcement choice word.
        auto choice = std::int32_t((r.frame_phase() >> 3) & 7);
        if (choice == r.announcement_voice)
            choice = add(choice, 1) & 7;
        r.announcement_voice = choice;
        if (scoring_) {
            constexpr int samples[] = {84, 96, 95, 97, 32, 33, 34, 31};
            constexpr int pans[] = {0, -64, -64, -64, 64, 64, 64, 64};
            scoring_->sound({samples[unsigned(choice)], pans[unsigned(choice)], 0, 1});
        }
    }
}
void TableMechanismRules::magnet(physics::SimulationState& simulation, TurnState& r, SharedGameState& s, bool collect_explosion) const {
    auto& p = s.players().at(std::uint32_t(r.current_player));
    if (edge(r, 8)) {
        if (r.station_super_stage() == StationSuperStage::magnetic_hole_required) {
            r.station_super_stage(StationSuperStage::ramp_collection_window);
            s.station_super_window = (6 * rule_time::second);
            if (scoring_)
                scoring_->sound({27, 64, 0, 1});
        }
        // Independent physical fixtures leave Explosion disconnected; the live
        // game runs its original pause/animation/wait and random award owner.
        if (!collect_explosion || p.awards.explosion_ready == 0 || r.drain_override || r.mission_time || r.single_ball_award_mode == 2) {
            bool activate = false;
            if (r.magnet_activation_window) {
                r.magnet_activation_window = 0;
                r.magnet_combo_timer = (6 * rule_time::second);
                activate = true;
            } else
                activate = p.magnet.hold != 0 && r.station_super_stage() < StationSuperStage::magnetic_hole_required;
            if (activate) {
                if (!r.super_launcher_timer) {
                    if (scoring_)
                        scoring_->sound({124, -64, 0, 1});
                    animation(68, 1);
                }
                // 0x40d75c: consume the available hold and arm the recovered
                // delayed force. BallMotion applies it only below (3 * rule_time::second).
                simulation.force_timer = (5 * rule_time::second);
                r.magnet_voice_timer = r.super_launcher_timer ? 0x15999 : 0x19999;
                p.magnet.hold = 0;
            }
        } else if (explosion_) {
            p.awards.explosion_ready = 2;
            r.explosion_wait = (3 * rule_time::half_second);
            r.returns_paused = 1;
            if (scoring_)
                scoring_->sound({168, 0, 0, 2});
            animation(87, 1);
        }
    }
    if (explosion_ && p.awards.explosion_ready == 2)
        explosion_();
    if (r.drain_override == 0) {
        if (edge(r, 3)) {
            LampRules{}.queue(r, s, 17, rule_time::half_second);
            r.magnet_activation_window = (4 * rule_time::second);
        }
        if (r.returns_paused == 0)
            timer(r, F::magnet_activation_window);
    }
    timer(r, F::magnet_combo_timer);
}
void TableMechanismRules::kickback(TurnState& r, SharedGameState& s) const {
    auto& p = s.players().at(std::uint32_t(r.current_player));
    LampRules lights;
    if ((edge(r, 2) || edge(r, 3)) && p.kickback.available == 0)
        r.kickback_rearm_timer = (4 * rule_time::second);
    timer(r, F::kickback_rearm_timer);
    if (edge(r, 6)) {
        lights.queue(r, s, 36, rule_time::quarter_second);
        if (r.kickback_rearm_timer) {
            r.kickback_rearm_timer = 0;
            if (p.kickback.available == 0) {
                restore_kickback(r, s, scoring_, animation_);
            } else if (scoring_) {
                scoring_->fill_next_rollover();
                scoring_->sound({178, -64, 32, 18});
            }
        } else if (scoring_)
            scoring_->sound({177, 64, 48, 17});
    }
    if (edge(r, 1) && p.kickback.available) {
        if (r.save_timer)
            r.auto_kick_enabled = rule_time::half_second;
        else {
            if (r.super_rescue == 0)
                p.kickback.available = 0;
            r.auto_kick_enabled = (8 * rule_time::second);
        }
    }
    // The original tests the pre-decrement timer, including an expiring tick.
    if (r.auto_kick_enabled) {
        timer(r, F::auto_kick_enabled);
        if (edge(r, 1)) {
            lights.queue(r, s, 14, rule_time::second);
            if (p.kickback.first_use_voice) {
                p.kickback.first_use_voice = 0;
                if (scoring_)
                    scoring_->sound({20, 64, 32, 1});
            } else if (scoring_)
                scoring_->sound({176, -64, 48, 18});
            if (!r.drain_override)
                animation(31, 0);
        }
    }
}
void TableMechanismRules::restore_kickback(TurnState& r, SharedGameState& s, ScoringRules* scoring, const Animation& draw) {
    s.players().at(std::uint32_t(r.current_player)).kickback.available = 1;
    if (scoring) {
        if (!r.kickback_announcement_played)
            scoring->sound({94, -64, 0, 1});
        scoring->sound({178, -64, 32, 18});
    }
    if (draw)
        draw(30, 0);
    r.kickback_announcement_played = 1;
}
void TableMechanismRules::outlane_extra_ball(TurnState& r) const {
    if (extra_balls_ && (edge(r, 1) || edge(r, 5)) && r.outlane_extra_ball) {
        r.outlane_extra_ball = 0;
        extra_balls_->award();
    }
}
void TableMechanismRules::outlanes(TurnState& r, SharedGameState& s) const {
    if (r.drain_override == 0) {
        if (edge(r, 5)) {
            if (scoring_)
                scoring_->sound({185, 64, 0, 18});
            LampRules{}.begin_animation(r, s, 1);
        }
        if (edge(r, 1) && r.auto_kick_enabled == 0) {
            if (scoring_)
                scoring_->sound({185, -64, 0, 18});
            LampRules{}.begin_animation(r, s, 1);
        }
    }
}
void TableMechanismRules::ramp(TurnState& r, SharedGameState& s) const {
    auto& p = s.players().at(std::uint32_t(r.current_player));
    if (scoring_)
        scoring_->ramp();
    r.launch_timer = 0;
    if (r.mission_time == 0 && r.drain_extension == 0) {
        r.drain_extension = 1;
        LampRules{}.queue(r, s, 53, rule_time::second);
    }
    // 0x407a27 calls 0x4089d4: the web ramp collects the lit fivefold/Ultra award.
    // The centre caller 0x410991 instead advances stages through 0x408770.
    // The live game connects Space Station; independent physical fixtures
    // explicitly leave that award callback disconnected.
    if (p.station.state == mode_state::active) {
        if (ramp_jackpot_)
            ramp_jackpot_();
        return;
    }
    if (p.station.state == mode_state::ready || r.magnet_combo_timer || r.station_ramp_ready || r.station_stage_code || p.launcher.state == mode_state::active)
        r.diverter_enabled = 0;
    else if (r.extra_ball_diverter)
        r.diverter_enabled = 1;
    else
        r.diverter_enabled = add(r.diverter_enabled, 1) & 1;
}
void TableMechanismRules::inlanes(TurnState& r) const {
    for (auto [field, contact] : {
             std::pair{F::inlane_a_timer, 13u},
             std::pair{F::inlane_b_timer, 14u}
    }) {
        if ((r.*field) == -1)
            continue;
        if (edge(r, contact)) {
            (r.*field) = (15 * rule_time::second);
            if (scoring_)
                scoring_->sound({174, contact == 13 ? -64 : 0, 32, 18});
        }
        timer(r, field);
    }
}
void TableMechanismRules::rollovers(TurnState& r, SharedGameState& s, std::uint32_t action_edges) const {
    auto& p = s.players().at(std::uint32_t(r.current_player));
    LampRules lights;
    for (unsigned i = 0; i < 3; i++)
        if (edge(r, 35 + i)) {
            lights.queue(r, s, 62 + i, rule_time::quarter_second);
            const bool unlit = p.rollovers.at(i) == 0;
            if (scoring_)
                scoring_->hundreds(unlit ? 5 : 1);
            if (unlit) {
                p.rollovers.at(i) = 1;
                if (scoring_)
                    scoring_->sound({181, -64, 64, 18});
            } else if (scoring_)
                scoring_->sound({173, -64, 96, 18});
        }
    if (p.rollovers[0] && p.rollovers[1] && p.rollovers[2]) {
        // 0x407ebc; the score service connects original bonus helper 0x407cb0.
        for (unsigned i = 0; i < 3; i++) {
            lights.queue(r, s, 62 + i, rule_time::second);
            p.rollovers.at(i) = 0;
        }
        if (scoring_)
            scoring_->advance_bonus();
    }
    if (action_edges & input_action::both_flippers) {
        const auto last = p.rollovers[2];
        p.rollovers[2] = p.rollovers[1];
        p.rollovers[1] = p.rollovers[0];
        p.rollovers[0] = last;
    }
}
void TableMechanismRules::step(physics::SimulationState& simulation, TurnState& r, SharedGameState& s, std::uint32_t action_edges) const {
    step_controls(simulation, r, s);
    rollovers(r, s, action_edges);
}
void TableMechanismRules::step_controls(physics::SimulationState& simulation, TurnState& r, SharedGameState& s) const {
    prepare_contacts(simulation, r, s);
    if (edge(r, 18))
        ramp(r, s);
    inlanes(r);
}
void TableMechanismRules::prepare_contacts(physics::SimulationState& simulation, TurnState& r, SharedGameState& s) const {
    magnet_timer(simulation, r, s);
    kickback(r, s);
    outlane_extra_ball(r);
    outlanes(r, s);
    magnet(simulation, r, s);
}
} // namespace web::rules
