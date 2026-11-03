#include "session_rules.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>

namespace web::rules {
using namespace physics;
using namespace physics::integer;
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;
namespace {
void sound(SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& sink, int id, int pan) {
    sink.on_effect(
        web::rules::SessionEffect{
            web::SoundRequest{id, pan, 0, 1}
    },
        simulation,
        rules,
        session);
}
void rule_display(SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& sink, int flags) {
    sink.on_effect(
        web::rules::SessionEffect{
            web::RuleDisplayRequest{58, flags, 1}
    },
        simulation,
        rules,
        session);
}
} // namespace
void SharedGameState::reset() {
    lamp_animation_enabled = 0;
    music_track = 0;
    dmd_queue_aux = 0;
    players_.at(0).records.saved_initials[0] = 0;
    web_ramp_combo_timer = 0;
    jet_gate_timer = 0;
    skyscraper_extended = 0;
    right_combo_entry_timer = 0;
    ammo_strength = 0;
    shuttle_charge = 0;
    right_loop_lamp_timer = 0;
    hunt_remaining = 0;
    jet_completion_window = 0;
    left_loop_lamp_timer = 0;
    mission_centre_lit = 0;
    reactor_remaining = 0;
    ammo_stage = 0;
    reactor_regeneration = 0;
    ammo_remaining = 0;
    shuttle_hits = 0;
    combo_mania_timer = 0;
    combo_count = 0;
    hunt_target = 0;
    mission_kind = 0;
    combo_remaining = 0;
    left_combo_entry_timer = 0;
    skyscraper_cooldown = 0;
    skyscraper_reset = 0;
    mode_reset = 0;
    left_spinner_timer = 0;
    station_super_holdoff = 0;
    station_super_window = 0;
    right_loop_completed = 0;
    right_loop_completion_timer = 0;
    initials_initialized = 0;
    loop_count = 0;
    initials_letters = 0;
    dmd_queue_state = 0;
    dmd_queue_count = 0;
    dmd_frame = 0;
    warning_display = 0;
    dmd_animation = 0;
    dmd_aux = 0;
    dmd_status = 0;
    music_revision_ = 0;
    lamp_queue = {};
    lamps.animation_cells.fill(0);
    lamps.sample_indices.fill(0);
    std::fill_n(lamps.outputs.begin(), 70, 0);
    lamps.drop_targets[0] = 0;
    for (auto& player : players_) {
        const auto relaunch = player.ball.relaunch;
        const auto records = player.records;
        player = Player(player.id());
        player.ball.relaunch = relaunch;
        player.records = records;
        player.records.replay_awarded = player.records.buyins = 0;
    }
}
void SessionRules::initialize_player(TurnState& rules, SharedGameState& session, std::size_t player) const {
    const auto cycle = rules.player_cycle;
    session.players().at(unsigned(player)).initialize(cycle);
    const auto next = add(cycle, 1);
    rules.player_cycle = next > 5 ? 0 : next;
}
void SessionRules::sample_lamps(SharedGameState& session) const {
    const auto cells = session.lamp_cells();
    for (std::size_t index = 0; index < 70; index++) {
        const auto cell = session.lamp_index(index);
        if (cell >= cells.size())
            throw std::out_of_range("Lamp lookup outside recovered grid");
        session.lamp_value(index, cells[cell] != 0);
    }
}
void SessionRules::clear_lamp_animation(TurnState& rules, SharedGameState& session) const {
    rules.lamp_animation = 0;
    session.lamp_animation_enabled = 0;
    auto cells = session.lamp_cells();
    std::fill(cells.begin(), cells.end(), 0);
    sample_lamps(session);
}
void SessionRules::stop_music(SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects) const {
    session.request_music_track(-1);
    effects.on_effect(web::rules::SessionEffect{web::MusicStopRequest{}}, simulation, rules, session);
}
void SessionRules::announce(SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects) const {
    auto voice = std::int32_t((rules.frame_phase() >> 4) & 3);
    if (voice == rules.announcement_voice)
        voice = (voice + 1) & 3;
    rules.announcement_voice = voice;
    static constexpr std::array<int, 4> samples = {90, 106, 105, 63};
    sound(simulation, rules, session, effects, samples[std::size_t(voice)], voice == 3 ? 0 : -64);
    rules.announce_pending = 0;
}
void SessionRules::tilt(SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects) const {
    if (rules.phase() != GamePhase::playing || rules.showdown_stage >= 4)
        return;
    if (rules.warnings_left != 0) {
        clear_lamp_animation(rules, session);
        rules.lamp_animation = 1;
        rules.lamp_mode = 2;
        rules.lamp_frame = 0;
        rules.warnings_left = sub(rules.warnings_left, 1);
        session.lamp_animation_enabled = 1;
        rule_display(simulation, rules, session, effects, 12);
        session.warning_display = rules.warnings_left;
        if (rules.warnings_left == 0) {
            // Original ANDs with 1 here (not the final tilt arm's mask 3).
            const auto voice = (rules.frame_phase() >> 4) & 1;
            sound(simulation, rules, session, effects, voice ? 46 : 45, 64);
        }
    } else if (rules.tilted == 0) {
        stop_music(simulation, rules, session, effects);
        rules.returns_paused = 0;
        rules.save_timer = 0;
        rules.launch_enabled = 0;
        simulation.mechanisms.enabled = 0;
        rules.tilted = 1;
        rules.tilt_lock = 1;
        session.warning_display = -1;
        rule_display(simulation, rules, session, effects, 256);
        const auto voice = (rules.frame_phase() >> 4) & 3;
        static constexpr std::array<int, 4> samples = {86, 70, 50, 103};
        sound(simulation, rules, session, effects, samples[voice], voice == 2 ? 64 : (voice == 3 ? -64 : 0));
    }
}
void SessionRules::setup_ball(SimulationState& simulation, TurnState& rules, SharedGameState& session) const {
    const auto player = rules.current_player;
    if (player < 0)
        throw std::out_of_range("Invalid current player");
    auto& p = session.players().at(std::size_t(player));
    auto queue = rules.lamp_queue;
    if (p.bonus.hold == 1)
        p.bonus.hold = 0;
    else
        p.bonus.multiplier = 1;
    if (p.bumpers.hold == 1)
        p.bumpers.hold = 0;
    else
        p.bumpers.super_jets = 0;
    p.power.level = 0;
    p.kickback.available = 1;
    if ((p.lock.state == mode_state::ready && p.lock.progress != 0) || p.lock.state == mode_state::active)
        p.lock.state = mode_state::inactive;
    for (auto* field :
         {&rules.showdown_intro,
          &rules.award_retrigger_guard,
          &rules.video_active,
          &rules.status_left_hold,
          &rules.status_right_hold,
          &rules.reserved_5,
          &rules.status_timer,
          &rules.status_selection,
          &rules.combined_lock,
          &rules.launch_timer,
          &rules.super_skillshot,
          &rules.flipper_timing_window,
          &rules.centre_award_wait,
          &rules.tilted,
          &rules.tilt_lock,
          &rules.reserved_2,
          &rules.video_lock_pending,
          &rules.returns_paused,
          &rules.station_super_lit,
          &rules.lock_choice_timer,
          &rules.serve_spacing_timer,
          &rules.super_launcher_timer,
          &rules.super_launcher_hits,
          &rules.showdown_phase,
          &rules.kickback_announcement_played,
          &rules.showdown_stage,
          &rules.explosion_wait,
          &rules.quick_shot_timer,
          &rules.quick_shot_side,
          &rules.diverter_enabled,
          &rules.auto_kick_enabled,
          &rules.web_ramp_entry_timer,
          &rules.loop_first_gate_timer,
          &rules.extra_ball_diverter,
          &rules.loop_remaining,
          &rules.station_triple_ready,
          &session.mode_reset,
          &rules.station_ramp_ready,
          &session.loop_count,
          &rules.magnet_voice_timer,
          &rules.magnet_activation_window,
          &rules.right_spinner_timer,
          &session.left_spinner_timer,
          &rules.left_loop_completion_timer,
          &rules.left_spinner_lane_timer,
          &rules.reserved_0,
          &rules.inlane_a_timer,
          &session.jet_completion_window,
          &session.left_combo_entry_timer,
          &session.right_loop_lamp_timer,
          &session.left_loop_lamp_timer,
          &session.combo_mania_timer,
          &rules.serve_requests,
          &rules.super_jet_total,
          &rules.outlane_extra_ball,
          &rules.super_rescue,
          &simulation.force_timer,
          &rules.return_toggle,
          &rules.ramp_a_entry_timer,
          &rules.loop_last_gate_timer,
          &rules.mission_time,
          &rules.station_centre_stage,
          &rules.station_stage_code,
          &rules.kickback_rearm_timer,
          &rules.magnet_combo_timer,
          &session.right_loop_completion_timer,
          &rules.right_spinner_lane_timer,
          &rules.reserved_1,
          &rules.inlane_b_timer,
          &session.jet_gate_timer,
          &session.web_ramp_combo_timer,
          &session.right_combo_entry_timer,
          &session.combo_count,
          &session.mission_centre_lit,
          &session.combo_remaining})
        *field = 0;
    rules.warnings_left = 3;
    rules.drain_extension = 1;
    rules.auto_launch_block = 0x10000;
    rules.target_bank_b_timer = 0x6000;
    rules.target_bank_a_timer = 0x4000;
    simulation.mechanisms.enabled = 1;
    if (p.ball.number == 3 && p.ball.extra_balls == 0 && p.score.rollovers == 0 && std::uint32_t(p.score.units) < 10000000) {
        p.lock.state = mode_state::ready;
        p.lock.balls = 3;
        p.lock.progress = 2;
        p.awards.explosion_ready = 1;
        if (queue < 0 || queue >= 8)
            throw std::out_of_range("Invalid lamp queue index");
        for (auto entry : {0x25, 0x1d}) {
            session.lamp_queue.at(unsigned(queue)).output = entry;
            session.lamp_queue.at(unsigned(queue)).remaining = rule_time::second;
            queue = (queue + 1) & 7;
        }
    }
    rules.current_player = player;
    rules.lamp_queue = queue;
}
SessionRules::StartResult SessionRules::start(SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects) const {
    if (rules.phase() == GamePhase::records)
        return StartResult::ignored;
    bool restarted = false;
    std::set<std::array<std::int32_t, 7>> visited;
    for (;;) {
        const auto current = rules.current_player, count = rules.player_count;
        const auto read_ball = [&] {
            if (current < 0)
                throw std::out_of_range("Invalid current player");
            return session.players().at(std::size_t(current)).ball.number;
        };
        if (!visited.insert({rules.phase_code, current, count, rules.start_latch, rules.drain_override, restarted ? 1 : 0, (current >= 0 && current < 4) ? read_ball() : 0}).second)
            throw std::domain_error("Original start/add-player loop repeated its state");
        if ((count == 0 || (count != 4 && read_ball() < 2)) && rules.start_latch != -1) {
            if (!restarted)
                rules.announce_pending = 1;
            if (count < 0)
                throw std::out_of_range("Invalid player count");
            initialize_player(rules, session, std::size_t(count));
            rules.player_count = add(count, 1);
            if (rules.start_latch == 0) {
                rules.start_latch = 1;
                for (unsigned i = 0; i < 8; i++) {
                    session.lamp_queue.at(i).output = 0;
                    session.lamp_queue.at(i).remaining = 0;
                }
                rules.lamp_queue = 0;
                rules.mission_flash = 0;
            }
            simulation.mechanisms.enabled = 1;
            if (rules.phase() == GamePhase::attract)
                rules.phase(GamePhase::begin_ball);
            return restarted ? StartResult::restarted : StartResult::player_added;
        }
        if (read_ball() > 1 && rules.drain_override == 0) {
            clear_lamp_animation(rules, session);
            rules.player_turn = 0;
            rules.current_player = 0;
            session.players().at(0).ball.number = 1;
            rules.player_count = 0;
            if (session.music_track != 10)
                stop_music(simulation, rules, session, effects);
            session.dmd_queue_count = 0;
            session.dmd_queue_state = 0;
            session.dmd_queue_aux = 0;
            session.dmd_animation = 0;
            session.dmd_frame = 0;
            session.dmd_status = dmd_status::table;
            session.dmd_aux = -1;
            setup_ball(simulation, rules, session);
            announce(simulation, rules, session, effects);
            rules.save_timer = 0x28000;
            if (rules.phase() > GamePhase::playing)
                rules.phase(GamePhase::attract);
            restarted = true;
        }
        if (!restarted)
            return StartResult::ignored;
    }
}
void SessionRuleCallbacks::on_event(const PhysicsEvent& event, BallSlots& balls) {
    if (&balls != &simulation_.balls)
        throw std::invalid_argument("Session callbacks require their owning ball slots");
    if (event.kind() == PhysicsEvent::Kind::tilt)
        SessionRules{}.tilt(simulation_, rules_, session_, effects_);
    else
        remaining_.on_event(event, balls);
}
} // namespace web::rules
