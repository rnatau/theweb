#include "lamps.hpp"
#include "ultimate.hpp"
#include "missions.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web::rules {
using namespace physics::integer;
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;
std::int32_t LampRules::value(const SharedGameState&, const LampState& lamps, unsigned lamp) {
    return lamps.outputs.at(lamp);
}
void LampRules::value(SharedGameState&, LampState& lamps, unsigned lamp, std::int32_t value) {
    lamps.outputs.at(lamp) = value;
}
std::array<std::int32_t, LampRules::count> LampRules::values(const SharedGameState&, const LampState& lamps) {
    return lamps.outputs;
}
void LampRules::initialize(TurnState& r, SharedGameState& s, LampState& l) const {
    static constexpr std::uint32_t lookup[] = {
#include "lamp_lookup.inc"
    };
    static_assert(std::size(lookup) == 70);
    for (unsigned i = 0; i < count; i++)
        value(s, l, i, 0);
    for (unsigned i = 0; i < 70; i++)
        s.lamp_index(i, lookup[i]);
    std::fill(s.lamp_cells().begin(), s.lamp_cells().end(), 0);
    r.lamp_animation = 1;
    r.lamp_mode = 99;
    r.lamp_frame = 0;
    (r.reserved_4 = 0); // Original initialization-only word; gameplay role unconfirmed.
}
void LampRules::clear_queue(TurnState& r, SharedGameState& s) const {
    for (unsigned i = 0; i < SharedGameState::lamp_queue_capacity; i++) {
        s.lamp_queue.at(i).remaining = 0;
        s.lamp_queue.at(i).output = 0;
    }
    r.lamp_queue = 0;
    r.mission_flash = 0;
}
void LampRules::queue(TurnState& r, SharedGameState& s, unsigned lamp, std::int32_t duration) const {
    if (lamp >= count)
        throw std::out_of_range("Invalid queued lamp");
    const auto head = std::uint32_t(r.lamp_queue);
    if (head >= SharedGameState::lamp_queue_capacity)
        throw std::out_of_range("Invalid lamp queue head");
    s.lamp_queue.at(head).output = std::int32_t(lamp);
    s.lamp_queue.at(head).remaining = duration;
    r.lamp_queue = (head + 1) % SharedGameState::lamp_queue_capacity;
}
void LampRules::step_queue(TurnState& r, SharedGameState& s, LampState& l) const {
    for (unsigned i = 0; i < SharedGameState::lamp_queue_capacity; i++)
        if (const auto time = s.lamp_queue.at(i).remaining; time != 0) {
            s.lamp_queue.at(i).remaining = std::max(0, sub(time, r.elapsed));
            value(s, l, std::uint32_t(s.lamp_queue.at(i).output), r.frame_phase() & 4);
        }
}
void LampRules::blink(const TurnState& r, SharedGameState& s, LampState& l, unsigned lamp, std::int32_t time) const {
    const auto mask = time <= (3 * rule_time::second) ? 4 : time <= (8 * rule_time::second) ? 8 : time <= (16 * rule_time::second) ? 16 : 32;
    value(s, l, lamp, time == -1 ? 1 : time <= rule_time::second ? 0 : std::int32_t(r.frame_phase() & mask));
}
void LampRules::begin_animation(TurnState& r, SharedGameState& s, std::int32_t mode) const {
    SessionRules{}.clear_lamp_animation(r, s);
    r.lamp_animation = 1;
    r.lamp_mode = mode;
    r.lamp_frame = 0;
    s.lamp_animation_enabled = 1;
}
void LampRules::step_animation(TurnState& r, SharedGameState& s) const {
    auto frame = r.lamp_frame;
    const auto mode = r.lamp_mode;
    auto cells = s.lamp_cells();
    const auto row = [&](int y, std::uint8_t v) {
        for (int x = 0; x < 32; x++)
            cells[(std::uint32_t(y) & 63) * 32 + x] = v;
    };
    const auto col = [&](int x) {
        for (int y = 0; y < 64; y++)
            cells[y * 32 + (std::uint32_t(x) & 31)] = 0;
    };
    const auto fill = [&](std::uint8_t v) { std::fill(cells.begin(), cells.end(), v); };
    const auto stop = [&] {
        s.lamp_animation_enabled = 0;
        r.lamp_animation = 0;
    };
    switch (mode) {
        case 0:
            if (frame < 64)
                row(sub(63, frame), 1);
            else if (frame < 128)
                col(frame >> 1);
            else if (frame < 192)
                row(frame, 1);
            else if (frame < 256)
                col(sub(31, frame >> 1));
            else
                frame = -1;
            break;
        case 1:
            if (frame < 64)
                row(frame, 1);
            else if (frame < 96) {
                auto y = mul(sub(frame, 32), 2);
                row(y, 0);
                row(add(y, 1), 0);
            } else if (frame >= 128)
                stop();
            break;
        case 2:
            if (frame < 24)
                fill(0);
            else
                stop();
            break;
        case 3:
            fill(0);
            break;
        case 99:
            if (frame < 24)
                fill(1);
            else if (frame < 48)
                fill(0);
            else if (s.dmd_status != dmd_status::startup) {
                frame = 0;
                r.lamp_mode = 0;
                s.lamp_animation_enabled = 0;
            }
            break;
        default:
            break;
    }
    if (r.frame_phase() != std::uint32_t(r.lamp_previous_frame_phase))
        frame = add(frame, 1);
    r.lamp_previous_frame_phase = bits(r.frame_phase());
    r.lamp_frame = frame;
    SessionRules{}.sample_lamps(s);
}
void LampRules::compose_quick_shot(const TurnState& r, SharedGameState& s, LampState& l) const {
    constexpr unsigned left[] = {32, 31, 23, 22, 21, 20};
    constexpr unsigned right[] = {35, 33, 27, 26, 25, 24};
    const auto* lamps = r.quick_shot_side ? right : left;
    for (unsigned i = 0; i < 6; ++i)
        value(s, l, lamps[i], (((r.frame_phase() + 4 * (i + 1)) >> 1) & 8) != 0);
}
void LampRules::compose_table(const physics::SimulationState& simulation, TurnState& r, SharedGameState& s, LampState& l, MissionRules* missions, UltimateRules* ultimate) const {
    auto& p = s.players().at(std::uint32_t(r.current_player));
    const auto out = [&](unsigned lamp, int v = 1) { value(s, l, lamp, v); };
    if (!r.tilt_lock && ((r.mission_time != 0 && !missions) || (r.showdown_phase == mode_state::active && !ultimate)))
        throw std::logic_error("Mission lamp dispatcher requires its active mode controller");
    for (unsigned i = 0; i < count; i++)
        out(i, 0);
    for (unsigned target = 0; target < l.drop_targets.size(); ++target)
        out(73 + target, l.drop_targets[target] == 0);
    if (r.tilt_lock)
        return;
    // Keep these stages in source order: later mode overlays take precedence.
    compose_award_inserts(simulation, r, s, l, p, ultimate);
    compose_objective_inserts(r, s, l, p);
    compose_shot_inserts(r, s, l, p);
    compose_control_inserts(r, s, l, p);
    compose_mode_inserts(r, s, l, p, missions);
}
void LampRules::compose_award_inserts(const physics::SimulationState& simulation, TurnState& r, SharedGameState& s, LampState& l, Player& p, UltimateRules* ultimate) const {
    const auto out = [&](unsigned lamp, int v = 1) { value(s, l, lamp, v); };
    const auto b = [&](unsigned lamp, int time) { blink(r, s, l, lamp, time); };
    if (simulation.force_timer) {
        if (r.frame_phase() & (simulation.force_timer < (3 * rule_time::second) ? 4 : 8)) {
            out(40);
            out(42);
        } else
            out(41);
    }
    if (r.extra_ball_diverter)
        b(54, r.extra_ball_diverter);
    else if (p.awards.extra_balls_lit)
        out(54);
    if (p.ball.extra_balls)
        out(0);
    if (!r.drain_override && r.single_ball_award_mode != mode_state::active) {
        if (p.station.state == mode_state::ready) {
            if (p.station.captures == 2)
                b(43, (8 * rule_time::second));
            else
                out(43);
        }
        if (r.magnet_combo_timer)
            b(43, r.magnet_combo_timer);
    }
    if (p.lock.state == mode_state::ready && !r.drain_override && !r.super_launcher_timer && !r.quick_shot_timer) {
        if (sub(p.lock.balls, 1) == p.lock.progress)
            b(29, (8 * rule_time::second));
        else
            out(29);
    }
    for (unsigned i = 0; i < 6; i++)
        if (p.targets.at(i))
            out(56 + i);
    constexpr unsigned power[] = {20, 24, 21, 25, 22, 26, 23, 27};
    const auto level = p.power.level;
    if (std::uint32_t(level) < 8)
        b(power[level], (8 * rule_time::second));
    for (int i = 0; i < std::min(level, 8); i++)
        out(power[i]);
    if (r.single_ball_award_mode == mode_state::ready)
        b(34, (8 * rule_time::second));
    if (r.showdown_phase == mode_state::ready) {
        b(10, (8 * rule_time::second));
        if (!r.drain_override)
            for (unsigned i : {55, 54, 53, 34})
                b(i, (8 * rule_time::second));
    }
    constexpr unsigned states[] = {12, 11, 9, 13, 8};
    const std::int32_t spheres[] = {p.power.sphere, p.combos.sphere, p.station.sphere, p.missions.sphere, p.bonus.sphere};
    if (r.showdown_phase == mode_state::active)
        ultimate->compose_lamps(l);
    else
        for (unsigned i = 0; i < 5; i++)
            if (spheres[i])
                out(states[i]);
}
void LampRules::compose_objective_inserts(TurnState& r, SharedGameState& s, LampState& l, Player& p) const {
    const auto out = [&](unsigned lamp, int v = 1) { value(s, l, lamp, v); };
    const auto b = [&](unsigned lamp, int time) { blink(r, s, l, lamp, time); };
    const auto bumpers = std::uint32_t(p.bumpers.super_jets);
    const auto time = p.bumpers.hold ? -1 : (17 * rule_time::second);
    if (bumpers) {
        b(70, time);
        if (bumpers > 1)
            b(71, time);
        if (bumpers > 2)
            b(72, time);
    }
    constexpr unsigned selections[] = {7, 6, 4, 3, 2, 5};
    if (!p.missions.all_played || r.mission_time)
        for (unsigned i = 0; i < 6; i++)
            if (p.missions.played.at(i))
                out(selections[i]);
    auto selected = p.missions.selected;
    auto selection_time = (8 * rule_time::second);
    if (p.missions.all_played && !r.mission_time) {
        if (!(r.frame_phase() & 3))
            selected = add(selected, 1);
        if (selected > 5)
            selected = 0;
        p.missions.selected = selected;
        selection_time = -1;
    }
    if (r.mission_time) {
        selected = s.mission_kind;
        selection_time = add(r.mission_time, rule_time::second);
    }
    if (std::uint32_t(selected) < 6)
        b(selections[selected], selection_time);
    if (!r.mission_time && !r.drain_override && !r.showdown_phase && r.drain_extension && !r.single_ball_award_mode)
        b(53, (8 * rule_time::second));
}
void LampRules::compose_shot_inserts(TurnState& r, SharedGameState& s, LampState& l, Player& p) const {
    const auto out = [&](unsigned lamp, int v = 1) { value(s, l, lamp, v); };
    const auto b = [&](unsigned lamp, int time) { blink(r, s, l, lamp, time); };
    if (r.station_super_stage() == StationSuperStage::ramp_collection_window)
        b(66, s.station_super_window);
    else if (r.station_ramp_ready)
        b(65, (8 * rule_time::second));
    if (p.station.state == mode_state::active)
        b(69, (8 * rule_time::second));
    if (p.lock.state == mode_state::active) {
        if (r.combined_lock) {
            for (unsigned i : {55, 52, 38, 30, 47, 44, 35, 32, 46})
                b(i, (16 * rule_time::second));
        } else {
            for (unsigned i = 0; i < 6; i++)
                if (p.targets.at(i)) {
                    b(69, (8 * rule_time::second));
                    break;
                }
        }
    }
    if (p.lock.state == mode_state::active || r.station_super_stage() == StationSuperStage::targets_required)
        for (unsigned i = 0; i < 6; i++)
            if (!p.targets.at(i))
                b(56 + i, (16 * rule_time::second));
    if (r.station_super_stage() == StationSuperStage::magnetic_hole_required)
        b(38, (16 * rule_time::second));
    if (r.loop_remaining)
        b(45, r.loop_remaining);
    if (!r.mission_time) {
        if (s.left_combo_entry_timer)
            b(35, s.left_combo_entry_timer);
        if (s.right_loop_lamp_timer)
            b(47, s.right_loop_lamp_timer);
        if (s.web_ramp_combo_timer)
            b(44, s.web_ramp_combo_timer);
        if (s.left_loop_lamp_timer) {
            b(46, s.left_loop_lamp_timer);
            b(44, s.left_loop_lamp_timer);
        }
        if (s.right_combo_entry_timer) {
            b(47, s.right_combo_entry_timer);
            b(32, s.right_combo_entry_timer);
        }
    }
    if (r.right_spinner_lane_timer)
        b(31, r.right_spinner_lane_timer);
    if (r.left_spinner_lane_timer)
        b(33, r.left_spinner_lane_timer);
    for (unsigned i = 0; i < 3; i++)
        out(62 + i, p.rollovers.at(i));
    if (r.inlane_a_timer)
        b(48, r.inlane_a_timer);
    if (r.inlane_b_timer)
        b(49, r.inlane_b_timer);
    if (r.station_super_stage() < StationSuperStage::magnetic_hole_required) {
        if (r.magnet_activation_window)
            b(39, r.magnet_activation_window);
        else
            out(39, p.magnet.hold);
    }
    if (p.magnet.ready)
        out(51);
}
void LampRules::compose_control_inserts(TurnState& r, SharedGameState& s, LampState& l, Player& p) const {
    const auto out = [&](unsigned lamp, int v = 1) { value(s, l, lamp, v); };
    const auto b = [&](unsigned lamp, int time) { blink(r, s, l, lamp, time); };
    if (!r.drain_override) {
        out(17);
        if (!r.quick_shot_timer) {
            out(16);
            out(18);
        }
    }
    if (!r.super_rescue) {
        if (r.auto_kick_enabled)
            b(14, r.auto_kick_enabled);
        else if (p.kickback.available)
            out(14);
    } else
        b(14, (16 * rule_time::second));
    if (r.kickback_rearm_timer) {
        if (!p.kickback.available)
            b(36, r.kickback_rearm_timer);
        else
            out(36);
    }
    if (s.mission_centre_lit)
        b(68, (8 * rule_time::second));
    if (s.jet_completion_window)
        b(67, s.jet_completion_window);
    if (!r.mission_time && !r.drain_override && r.single_ball_award_mode != mode_state::active) {
        if (p.awards.explosion_ready)
            out(37);
        if (p.video.ready)
            out(28);
    }
    if (r.outlane_extra_ball) {
        out(15);
        out(19);
    }
}
void LampRules::compose_mode_inserts(TurnState& r, SharedGameState& s, LampState& l, Player& p, MissionRules* missions) const {
    const auto b = [&](unsigned lamp, int time) { blink(r, s, l, lamp, time); };
    if (r.mission_flash) {
        const auto remaining = std::max(0, sub(r.mission_flash_time, r.elapsed));
        r.mission_flash_time = remaining;
        if (!remaining)
            r.mission_flash = 0;
    }
    if (r.mission_time && missions)
        missions->compose_lamps();
    if (p.quick_shot.state == 2)
        compose_quick_shot(r, s, l);
    if (p.combos.state == 2)
        for (unsigned i : {47, 44, 35, 32, 46})
            b(i, (16 * rule_time::second));
    if (p.launcher.state == mode_state::active) {
        p.magnet.hold = 1;
        const auto t = sub(r.super_launcher_timer, (3 * rule_time::second));
        if (t > 0) {
            b(39, t);
            b(44, t);
        }
    }
    if (r.launch_timer)
        b(44, r.launch_timer);
    if (r.station_super_lit)
        b(50, (8 * rule_time::second));
    if (r.save_timer > (2 * rule_time::second))
        b(1, sub(r.save_timer, (2 * rule_time::second)));
    step_queue(r, s, l);
}
} // namespace web::rules
