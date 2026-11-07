#include "gameplay.hpp"
#include "physics/integer.hpp"
#include "rules/idle.hpp"
#include "rules/music_policy.hpp"
#include <algorithm>
#include <stdexcept>

namespace web {
using namespace physics;
namespace F = web::rules::turn_field;
using Switch = rules::TableSwitch;
using namespace physics::integer;
PinballGame::PinballGame(
    const std::filesystem::path& assets,
    InputTiming timing,
    rules::StartAction start_action,
    RestartGesture restart_gesture,
    rules::CollectionPolicy collection_policy,
    GameCorrections corrections) :
    corrections_(corrections),
    physics_(PhysicsSimulation::load(assets, corrections_.physics)),
    display_(assets, corrections_.fit_hideout_caption),
    original_input_order_(timing != InputTiming::post_poll_fixture),
    timestamped_input_(timing == InputTiming::timestamped),
    start_action_(start_action),
    restart_gesture_(std::move(restart_gesture)),
    collection_policy_(collection_policy) {
    simulation_.balls = initial_balls();
    if (corrections_.initialize_mechanisms)
        corrections_.initialize_mechanisms(simulation_.mechanisms);
    lamp_rules_.initialize(game_state_.turn, game_state_.shared, game_state_.shared.lamps);
    drop_targets_.initialize(game_state_.shared, game_state_.shared.lamps);
    scoring_.music_resume_handler([this] { resume_music(); });
    if (corrections_.improved_records)
        bonus_.first_buyin_handler([this](unsigned player, rules::ScoreValue score) { game_state_.players.at(player).records.pre_buyin = score; });
}
void PinballGame::initialize_engine_startup() {
    if (frames_ || phase_ != Phase::ready)
        throw std::logic_error("Engine startup must precede simulation");
    game_state_.turn.elapsed = rules::rule_time::second / simulation_.refresh_rate;
    game_state_.turn.start_latch = -1;
    game_state_.turn.startup_display_ticks = 24;
    game_state_.shared.dmd_status = rules::dmd_status::startup;
    display_.initialize_startup();
}
void PinballGame::resume_music() {
    rules::MusicPolicy(game_state_.turn, game_state_.shared, simulation_, scoring_.sound_sink()).step(std::int32_t(music_restart_policy_));
}
bool PinballGame::collection_allowed() const {
    return !collection_policy_ || collection_policy_(game_state_.turn, game_state_.shared);
}
bool PinballGame::begin_presentation(const std::function<void(std::uint32_t)>& nudge_display) {
    const auto nudge = pending_nudge_feedback_;
    pending_nudge_feedback_ = 0;
    // Original main loop 0x4140ad: the same rubber hit, in two distinct groups.
    if (nudge) {
        if (nudge_display)
            nudge_display(nudge);
        scoring_.sound({190, 0, 0, 17});
        scoring_.sound({190, 0, 0, 18});
    }
    // 0x413964 tests the preceding poll's start edge before polling new input.
    // Unlike launch, S therefore belongs to the following presentation pass.
    if (pending_start_) {
        pending_start_ = false;
        start();
    }
    return nudge;
}
void PinballGame::start() {
    if (restart_pending_)
        return;
    const bool idle = phase_ == Phase::ready || phase_ == Phase::game_over;
    const auto result = start_action_ ? start_action_(simulation_, game_state_.turn, game_state_.shared, *this) : session_rules_.start(simulation_, game_state_.turn, game_state_.shared, *this);
    if (result == rules::SessionRules::StartResult::rejected) {
        ++rejected_starts_;
        return;
    }
    if (result == rules::SessionRules::StartResult::ignored)
        return;
    if (corrections_.improved_records && (idle || result == rules::SessionRules::StartResult::restarted)) {
        end_state_.records = {};
        game_state_.players.reset_record_eligibility();
    }
    if (idle) {
        // Original start initializes the selected player, rather than resetting
        // the engine. Retain physical motion, player cycle, RNG and shared state.
        frames_ = interactions_ = drains_ = launches_ = 0;
    }
    if (result == rules::SessionRules::StartResult::restarted) {
        // Original 0x4053f0 clears the display queue/status on a restart but
        // retains the physical ball, shared RNG and global controller state.
        display_.stop_animations();
        display_.clear_page();
        ball_number_ = unsigned(game_state_.shared.players().at(current_player()).ball.number);
    }
    if (game_state_.turn.phase() == rules::GamePhase::begin_ball)
        begin_ball();
}
void PinballGame::drain_for_restart() {
    restart_pending_ = true;
    pending_start_ = false;
    pending_nudge_feedback_ = 0;
    // Let the existing balls finish their physical journey with the flippers
    // down. Disable saves, kickbacks and captures while abandoning the game.
    game_state_.turn.save_timer = 0;
    game_state_.turn.serve_requests = 0;
    game_state_.turn.returns_paused = 0;
    game_state_.turn.tilt_lock = 1;
    game_state_.turn.auto_kick_enabled = 0;
    // The shooter fires on its normal physical contact, including balls still
    // travelling down the lane. Do not open the player-launch save window.
    game_state_.turn.launch_enabled = 0;
    game_state_.turn.auto_launch_block = 0;
    simulation_.actions = 0;
    simulation_.force_timer = 0;
    simulation_.nudge = {};
    scoring_.stop_sounds();
    for (std::size_t slot = 0; slot < simulation_.balls.size(); ++slot) {
        auto& ball = simulation_.balls[slot];
        if (!ball.active() || ball.motion_type() == ball_type::pending_drain || ball.motion_mode() == motion_mode::draining)
            continue;
        if (ball.motion_type() == ball_type::launcher)
            continue;
        if (ball.motion_type() != ball_type::free || ball.motion_mode() == motion_mode::held || ball.motion_mode() == motion_mode::station_hold) {
            const auto exit = ball.motion_type() == ball_type::lift_return                                     ? rules::BallLifecycle::Exit::lift
                              : ball.motion_type() == ball_type::upper_return || ball.position().x >= 0xa00000 ? rules::BallLifecycle::Exit::right
                                                                                                               : rules::BallLifecycle::Exit::left;
            lifecycle_.eject(simulation_, game_state_.turn, slot, exit, *this);
        }
    }
    // Original SFX_ID_E_BYEB: E1400001.pcm, through normal mixer arbitration.
    constexpr int farewell_sound = 81;
    scoring_.sound({farewell_sound, 0, 0, 1});
    // Presentation enters pre-game immediately; physical clear-down and Start
    // eligibility remain owned by the retained session.
    display_.reset();
    update_display_contexts(0, 0);
    DotMatrixRenderer::ScorePage idle;
    idle.started = false;
    display_.advance(0, idle);
}
bool PinballGame::restart_ready() const {
    return restart_requested_ && restart_drained();
}
bool PinballGame::restart_drained() const {
    // Give the farewell (0.87 s) and the final physical drain a short settle.
    return restart_pending_ && restart_frames_ >= unsigned(simulation_.refresh_rate) &&
           std::none_of(simulation_.balls.begin(), simulation_.balls.end(), [](const auto& ball) { return ball.active() != 0; });
}
void PinballGame::begin_ball() {
    // The original phase-one handoff retains the final bonus request until
    // its independent DMD timer expires. Start/restart owns explicit clears.
    // Actual phases one/two execute on their own following rule updates.
    ball_number_ = unsigned(game_state_.shared.players().at(unsigned(game_state_.turn.player_turn)).ball.number);
    game_state_.turn.phase(rules::GamePhase::begin_ball);
    phase_ = Phase::playing;
}
void PinballGame::timers_and_serve(std::uint32_t edges) {
    display_.animation_sounds(scoring_.sound_sink());
    const auto elapsed = game_state_.turn.elapsed;
    const auto timer = [&](web::rules::TurnMember field) {
        auto value = (game_state_.turn.*field);
        if (value)
            (game_state_.turn.*field) = std::max(0, sub(value, elapsed));
    };
    // Original 0x40e890: animation cues precede physical timer/serve updates.
    timer(F::launch_timer);
    if (game_state_.turn.switch_word(38) == 0)
        game_state_.turn.auto_launch_block = rules::rule_time::second;
    timer(F::auto_launch_block);
    if (simulation_.force_timer) {
        if (simulation_.double_steps)
            simulation_.force_timer = std::max(0, sub(simulation_.force_timer, elapsed));
        simulation_.force_timer = std::max(0, sub(simulation_.force_timer, elapsed));
        if (simulation_.force_timer < 3 * rules::rule_time::second && (edges & input_action::left_flipper))
            simulation_.force_timer = 0;
    }
    timer(F::serve_spacing_timer);
    if (game_state_.turn.serve_requests != 0 && game_state_.turn.serve_spacing_timer == 0) {
        lifecycle_.serve(simulation_, game_state_.turn, *this);
        game_state_.turn.serve_requests = sub(game_state_.turn.serve_requests, 1);
        game_state_.turn.serve_spacing_timer = 3 * rules::rule_time::half_second;
    }
    game_state_.turn.sample_switches();
}
void PinballGame::step(std::uint32_t actions) {
    step_frame(actions, nullptr);
}
void PinballGame::step_timed(const TimedInputFrame& input) {
    if (!timestamped_input_) {
        step(input.rule_actions);
        return;
    }
    if (input.microsteps.size() != PhysicsSimulation::microstep_count(simulation_.refresh_rate, simulation_.double_steps))
        throw std::invalid_argument("Timed input does not match the physics microstep count");
    step_frame(input.rule_actions, &input);
}
void PinballGame::begin_frame(std::uint32_t actions, const TimedInputFrame* input) {
    if (input && (input->released & input_action::start) && restart_gesture_)
        restart_gesture_(0, game_state_.turn, simulation_.refresh_rate);
    const auto gesture_actions = input ? input->microsteps.back().actions : actions;
    if (!restart_pending_ && restart_gesture_ && restart_gesture_(gesture_actions, game_state_.turn, simulation_.refresh_rate))
        drain_for_restart();
    if (restart_pending_) {
        const auto start_edge = (input ? input->pressed : actions & ~previous_actions_) & input_action::start;
        if (!(gesture_actions & input_action::start) || (input && (input->released & input_action::start)))
            restart_start_armed_ = true;
        // Discard presses during clear-down. Standby accepts only a new Start
        // edge after the old balls have finished, never the original hold.
        if (restart_start_armed_ && start_edge && restart_drained())
            restart_requested_ = true;
        previous_actions_ = gesture_actions;
        ++restart_frames_;
        ++frames_;
        game_state_.turn.elapsed = rules::rule_time::second / simulation_.refresh_rate;
        lifecycle_.update_returns(simulation_, game_state_.turn, *this);
        actions = 0;
    }
    frame_edges_ = restart_pending_ ? 0 : input ? input->pressed : actions & ~previous_actions_;
    frame_actions_ = actions;
    if (!restart_pending_)
        step_rules(actions, input);
}
void PinballGame::apply_physical_input(TimedInputSample sample, bool first) {
    constexpr auto physical = input_action::both_flippers | input_action::any_nudge | input_action::launch | input_action::ball_control;
    const auto changed = first ? physical : (previous_physical_actions_ ^ sample.actions) & physical;
    simulation_.actions = (simulation_.actions & ~changed) | (sample.actions & changed);
    previous_physical_actions_ = sample.actions;
    if (const auto nudges = sample.pressed & input_action::any_nudge) {
        pending_nudge_feedback_ |= nudges;
        simulation_.nudge.tilt = sub(simulation_.nudge.tilt, 16);
    }
    if (sample.pressed & input_action::launch)
        lifecycle_.launch(simulation_, game_state_.turn, *this);
}
void PinballGame::step_frame(std::uint32_t actions, const TimedInputFrame* input) {
    if (remaining_microsteps())
        throw std::logic_error("Cannot batch an unfinished game tick");
    begin_frame(actions, input);
    if (!input || restart_pending_)
        physics_.step(simulation_, *this);
    else {
        previous_physical_actions_ = simulation_.actions;
        std::size_t previous_index = input->microsteps.size();
        physics_.step(simulation_, *this, [&](unsigned tick, unsigned count) {
            // Rules may double this tick's count. Retain historical TWI1 bins.
            const auto index = std::size_t(tick) * input->microsteps.size() / count;
            auto sample = input->microsteps[index];
            if (index == previous_index)
                sample.pressed = 0;
            previous_index = index;
            apply_physical_input(sample, tick == 0);
        });
    }
    finish_frame(frame_actions_, frame_edges_);
}
unsigned PinballGame::begin_tick(const TimedInputFrame& input) {
    if (!timestamped_input_ || remaining_microsteps() || input.microsteps.empty())
        throw std::logic_error("Invalid incremental game tick boundary");
    begin_frame(input.rule_actions, &input);
    tick_microsteps_ = PhysicsSimulation::microstep_count(simulation_.refresh_rate, simulation_.double_steps);
    microstep_index_ = 0;
    previous_physical_actions_ = simulation_.actions;
    if (!tick_microsteps_)
        finish_frame(frame_actions_, frame_edges_);
    return tick_microsteps_;
}
bool PinballGame::step_microstep(TimedInputSample input) {
    if (!remaining_microsteps())
        throw std::logic_error("No incremental game tick is active");
    if (!restart_pending_)
        apply_physical_input(input, microstep_index_ == 0);
    physics_.step_microstep(simulation_, *this);
    if (++microstep_index_ != tick_microsteps_)
        return false;
    finish_frame(frame_actions_, frame_edges_);
    return true;
}
void PinballGame::finish_frame(std::uint32_t actions, std::uint32_t display_edges) {
    std::optional<DmdDisplay::VideoScene> video_scene;
    if (!restart_pending_ && game_state_.turn.video_active && game_state_.shared.dmd_status == rules::dmd_status::video && display_.accepts_video_scene()) {
        // The game owns mini-game mutations. Sample the pending DMD frame
        // before the update can request a result page or queue new artwork.
        video_.step(actions);
        video_scene = DmdDisplay::VideoScene{video_state_, score(), game_state_.turn.video_active != 0};
    }
    DotMatrixRenderer::ScorePage page;
    page.player = current_player();
    page.players = std::max(1u, player_count());
    for (unsigned i = 0; i < 4; ++i) {

        page.balls[i] = game_state_.shared.players().at(i).ball.number;
        page.scores[i] = {std::uint32_t(game_state_.shared.players().at(i).score.units), std::uint32_t(game_state_.shared.players().at(i).score.rollovers)};
    }
    page.credits = scoring_.replay().credits;
    page.repeated_simulation = simulation_.double_steps != 0;
    page.started = !restart_pending_ && phase_ != Phase::ready && phase_ != Phase::game_over;
    std::optional<DotMatrixRenderer::BonusPage> bonus_page;
    {
        // Selector 57 can outlive phase four. Its values still follow the
        // completed rules update, including the final blank stage.

        const auto& player = game_state_.shared.players().at(current_player());
        bonus_page = DotMatrixRenderer::BonusPage{
            bonus_display_,
            player.bonus.loops,
            player.bonus.power_levels,
            player.bonus.combos,
            player.bonus.multiplier,
            game_state_.turn.bonus_timer,
            player.bonus.hold,
            scoring_.replay().credits};
    }
    update_display_contexts(actions, display_edges);
    // Presentation has its own recovered cadence in every rules phase.
    const bool startup = game_state_.shared.dmd_status == rules::dmd_status::startup;
    const bool preparing_startup = startup && display_.pending_frame();
    display_.advance(rules::rule_time::second / simulation_.refresh_rate, page, bonus_page, video_scene, startup);
    if (preparing_startup) {
        const auto remaining = sub(game_state_.turn.startup_display_ticks, 1);
        game_state_.turn.startup_display_ticks = remaining;
        if (!remaining) {
            game_state_.turn.start_latch = 0;
            game_state_.shared.dmd_status = rules::dmd_status::table;
        }
    }
}
void PinballGame::update_display_contexts(std::uint32_t actions, std::uint32_t display_edges) {
    display_.mission_context(game_state_.turn.mission_total, game_state_.turn.skyscraper_floor, game_state_.shared.players().at(current_player()).missions.rounds);
    display_.scoring_context(std::uint32_t(game_state_.shared.players().at(current_player()).awards.ramp_value), game_state_.turn.super_jet_total, game_state_.turn.super_skillshot != 0);
    display_.video_context(std::uint32_t(video_state_.total), video_state_.perfect != 0);
    const auto& display_player = game_state_.shared.players().at(current_player());

    display_.status_context(
        {display_player.bonus.multiplier,
         display_player.bonus.hold,
         display_player.ball.extra_balls,
         display_player.awards.extra_balls_lit,
         display_player.loops.extra_ball_threshold,
         display_player.showdown.spheres});
    display_.lock_context(game_state_.turn.lock_choice, bonus_display_.continue_seconds, sub(display_player.lock.balls, display_player.lock.progress));
    display_.ultimate_context(ultimate_.display_values());
    const auto& replay = scoring_.replay();
    display_.serve_context({
        unsigned(game_state_.turn.player_turn),
        display_player.records.replay_awarded != 0,
        display_player.records.buyins != 0,
        {replay.threshold_units, std::uint32_t(replay.threshold_rollovers)},
        records_.ordinary()[0].score,
        records_.buyin()[1].score
    });
    const auto entry_player = std::clamp(game_state_.turn.record_entry_player, 0, 3);
    const auto letters = std::uint32_t(game_state_.shared.initials_letters);
    display_.record_entry_context({
        unsigned(entry_player),
        unsigned(game_state_.turn.initials_cursor),
        actions,
        {std::uint8_t(letters), std::uint8_t(letters >> 8), std::uint8_t(letters >> 16)}
    });
    const rules::ScoreValue record_score = corrections_.improved_records && end_state_.records.entering_pre_buyin
                                               ? rules::ScoreValue{std::uint32_t(game_state_.turn.record_best_low), std::uint32_t(game_state_.turn.record_best_high)}
                                               : rules::ScoreValue{
                                                     std::uint32_t(game_state_.shared.players().at(unsigned(entry_player)).score.units),
                                                     std::uint32_t(game_state_.shared.players().at(unsigned(entry_player)).score.rollovers)};
    display_.record_context({unsigned(entry_player), unsigned(game_state_.turn.record_display_rank), int(records_.combos().count), int(records_.loops().count), record_score});
    DotMatrixRenderer::MatchValues match;
    match.players = unsigned(std::max(1, game_state_.turn.player_count));
    match.digit = game_state_.turn.match_digit;
    match.stage = game_state_.turn.match_stage;
    for (unsigned i = 0; i < 4; ++i)
        match.score_units[i] = std::uint32_t(game_state_.shared.players().at(i).score.units);
    display_.match_context(match);
    AttractValues attract;
    attract.held = actions;
    attract.edges = display_edges;
    attract.previous_players = end_state_.previous_players;
    attract.credits = replay.credits;
    attract.repeated_simulation = simulation_.double_steps != 0;
    attract.replay = {replay.threshold_units, std::uint32_t(replay.threshold_rollovers)};
    for (unsigned i = 0; i < 4; ++i)
        attract.scores[i] = {std::uint32_t(game_state_.shared.players().at(i).score.units), std::uint32_t(game_state_.shared.players().at(i).score.rollovers)};
    attract.records = records_;
    display_.attract_context(std::move(attract));
}
void PinballGame::step_rules(std::uint32_t actions, const TimedInputFrame* timed) {
    auto edges = timed ? timed->pressed : actions & ~previous_actions_;
    previous_actions_ = timed ? timed->microsteps.back().actions : actions;
    if (edges & input_action::start) {
        if (original_input_order_)
            pending_start_ = true;
        else
            start(); // Explicit controlled post-poll boundary for headless callers.
    }
    simulation_.actions = actions;
    game_state_.turn.elapsed = rules::rule_time::second / simulation_.refresh_rate;
    auto repeats = input_repeat_.step(actions, edges);
    game_state_.turn.frame_phase(game_state_.turn.frame_phase() + 1);
    if (!timed && (edges & input_action::any_nudge)) {
        pending_nudge_feedback_ |= edges & input_action::any_nudge;
        simulation_.nudge.tilt = sub(simulation_.nudge.tilt, 16); // Original pre-frame 0x401054.
    }
    if (!timed && (edges & input_action::launch))
        lifecycle_.launch(simulation_, game_state_.turn, *this);      // Original input poll precedes rules.
    (game_state_.turn.current_player = game_state_.turn.player_turn); // Original 0x411c58.
    if (pending_effects_volume_) {
        if (game_state_.shared.dmd_status != rules::dmd_status::startup)
            display_.effects_volume(*pending_effects_volume_);
        pending_effects_volume_.reset();
    }
    game_end_.feedback();
    lifecycle_.update_returns(simulation_, game_state_.turn, *this);
    if (phase_ == Phase::ready || phase_ == Phase::game_over) {
        rules::IdleRules{}.step(simulation_, game_state_.turn, game_state_.shared, game_state_.shared.lamps);
        return;
    }
    ++frames_;
    if (phase_ == Phase::ending) {
        game_end_.step(edges, repeats);
        if (game_state_.turn.phase() == rules::GamePhase::attract)
            phase_ = Phase::game_over;
        return;
    }
    if (phase_ == Phase::between_balls) {
        if (game_state_.turn.phase() == rules::GamePhase::begin_ball) {
            begin_ball();
        } else {
            game_state_.turn.mission_time = 0;
            compose_lamps();
            bonus_.step(edges, actions);
            display_.show_bonus();
            if (game_state_.turn.phase() == rules::GamePhase::records) {
                phase_ = Phase::ending;
            }
            return;
        }
    }
    if (game_state_.turn.phase() == rules::GamePhase::begin_ball || game_state_.turn.phase() == rules::GamePhase::serve_ball) {
        serve_.step();
        return;
    }
    if (game_state_.turn.announce_pending)
        session_rules_.announce(simulation_, game_state_.turn, game_state_.shared, *this);
    timers_and_serve(edges);
    if (!game_state_.turn.tilted) {
        step_active_play(edges, actions);
    }
    compose_lamps();
    if (game_state_.turn.live_balls == 0 && game_state_.turn.serve_requests == 0) {
        if (game_state_.turn.showdown_stage > 3)
            ultimate_.finish();
        else {
            phase_ = Phase::between_balls;
            display_.stop_animations();
            display_.clear_page();
            bonus_.begin();
            return;
        }
    }
}
void PinballGame::step_active_play(std::uint32_t& edges, std::uint32_t actions) {
    if (!game_state_.turn.video_active)
        rules::StatusRules{}.step(game_state_.turn, edges, actions, &display_);
    single_ball_awards_.refresh_mode(edges);
    ultimate_.ready();
    resume_music();
    if (const auto remaining = game_state_.turn.save_timer)
        game_state_.turn.save_timer = std::max(0, sub(remaining, game_state_.turn.elapsed));
    table_mechanisms_.magnet_timer(simulation_, game_state_.turn, game_state_.shared);
    explosion_.timing_shot(edges, actions);
    // Original 0x40ec0c follows the timing award and common serve/latch
    // update. Video may consume flipper edges, but only after that award.
    if (game_state_.turn.video_active) {
        game_state_.shared.dmd_animation = std::int32_t(display_.remaining_ticks());
        video_.input(edges, actions);
    }
    station_.cleanup();
    lock_awards_.cleanup();
    ultimate_.cleanup();
    loop_combos_.step_loops();
    table_mechanisms_.kickback(game_state_.turn, game_state_.shared);
    table_mechanisms_.outlane_extra_ball(game_state_.turn);
    table_mechanisms_.outlanes(game_state_.turn, game_state_.shared);
    table_mechanisms_.magnet(simulation_, game_state_.turn, game_state_.shared, collection_allowed());
    shot_sequences_.step();
    ramp_awards_.step_contacts(table_mechanisms_);
    table_mechanisms_.inlanes(game_state_.turn);
    scoring_.slings();
    power_levels_.step_jets();
    scoring_.bumpers();
    for (auto contact : {Switch::bumper_a, Switch::bumper_c, Switch::bumper_b})
        if (game_state_.turn.switch_edge(contact))
            missions_.advance_selection();
    table_mechanisms_.rollovers(game_state_.turn, game_state_.shared, edges);
    if (edges & input_action::both_flippers)
        mission_state_.scratch = game_state_.shared.players().at(current_player()).rollovers[0];
    loop_combos_.step_combos();
    station_.upper_hole();
    interactions_ += drop_targets_.step(game_state_.turn, game_state_.shared, game_state_.shared.lamps);
    const bool upper_contact = game_state_.turn.switch_edge(Switch::sinkhole_b);
    // The original jumps around the lock arm when video is selected. A
    // lock start may clear mission_time, so do not reconsider that gate.
    const bool starts_video = lock_awards_.upper_hole(collection_allowed());
    video_.step_contacts(starts_video && collection_allowed());
    if (upper_contact && game_state_.turn.station_triple_ready == 2)
        station_.triple_jackpot();
    if (game_state_.turn.lock_choice_timer)
        lock_awards_.choose(edges);
    advance_missions();
}
void PinballGame::compose_lamps() {
    if (game_state_.turn.lamp_animation)
        lamp_rules_.step_animation(game_state_.turn, game_state_.shared);
    else
        lamp_rules_.compose_table(simulation_, game_state_.turn, game_state_.shared, game_state_.shared.lamps, &missions_, &ultimate_);
}
void PinballGame::advance_missions() {
    centre_shot_.step(collection_allowed());
    if (game_state_.turn.showdown_phase == 1 && game_state_.turn.showdown_intro)
        ultimate_.introduction();
    if (game_state_.turn.showdown_phase > 1)
        ultimate_.step();
    const auto& p = game_state_.shared.players().at(current_player());
    const bool single_ball_arm = p.launcher.state == 2 || p.combos.state == 2 || p.quick_shot.state == 2;
    single_ball_awards_.step(simulation_.refresh_rate);
    if (!single_ball_arm)
        lock_awards_.combined();
    // Original 0x410a5d skips objective updates on the mission-starting frame.
    if (!game_state_.turn.mission_started_this_frame)
        missions_.step();
    station_.timers();
    if (game_state_.turn.extra_ball_diverter)
        (game_state_.turn.return_toggle = 1); // Original tail 0x410ae9.
}
void PinballGame::on_centre_request(rules::CentreRequest request) {
    if (request == rules::CentreRequest::unlit_hole)
        display_.unlit_hole();
    else if (request == rules::CentreRequest::start_single_ball_award)
        single_ball_awards_.start();
    else if (request == rules::CentreRequest::collect_lock_multiball)
        lock_awards_.jackpot();
    else if (request == rules::CentreRequest::collect_ramp_multiball)
        station_.centre_jackpot();
    else if (request == rules::CentreRequest::start_showdown)
        ultimate_.start();
}
void PinballGame::on_event(const PhysicsEvent& event, BallSlots& balls) {
    if (&balls != &simulation_.balls)
        throw std::invalid_argument("Game callback ball ownership mismatch");
    if (restart_pending_) {
        // Physical contacts remain audible, but cannot score, save a ball or
        // start another mode. Holes swallow their ball instead of returning it.
        if (event.kind() == PhysicsEvent::Kind::drain) {
            balls.at(event.slot).active(0);
            ++drains_;
        } else if (event.kind() == PhysicsEvent::Kind::surface_rule) {
            const auto transition = event.get<SurfaceContact>().transition;
            // The dock cannot capture another ball. The shooter retains its
            // normal automatic launch so balls drain through the playfield.
            lifecycle_.surface_transition(simulation_, game_state_.turn, event.slot, transition == 11 ? 0 : transition, *this);
        } else if (event.kind() == PhysicsEvent::Kind::sound)
            scoring_.sound(event.get<SoundRequest>());
        return;
    }
    switch (event.kind()) {
        case PhysicsEvent::Kind::drain:
            lifecycle_.drained(simulation_, game_state_.turn, event.slot);
            break;
        case PhysicsEvent::Kind::switch_contact:
            game_state_.turn.switch_contact(event.get<SwitchContact>().switch_id);
            break;
        case PhysicsEvent::Kind::surface_rule:
            lifecycle_.surface_transition(simulation_, game_state_.turn, event.slot, event.get<SurfaceContact>().transition, *this);
            break;
        case PhysicsEvent::Kind::tilt:
            session_rules_.tilt(simulation_, game_state_.turn, game_state_.shared, *this);
            break;
        case PhysicsEvent::Kind::spinner_crossing:
            ++interactions_;
            scoring_.spinner(unsigned(event.get<SpinnerCrossing>().spinner));
            break;
        case PhysicsEvent::Kind::sound:
            scoring_.sound(event.get<SoundRequest>());
            break;
    }
}
void PinballGame::on_effect(const rules::BallRuleEffect& effect, SimulationState&, rules::TurnState&) {
    if (effect.kind() == rules::BallRuleEffect::Kind::station_contact) {
        if (collection_allowed())
            station_.capture(effect.slot);
        return;
    }
    if (effect.kind() == rules::BallRuleEffect::Kind::display) {
        display_.animation(unsigned(effect.get<AnimationRequest>().id), std::uint32_t(effect.get<AnimationRequest>().flags));
        return;
    }
    if (effect.kind() != rules::BallRuleEffect::Kind::sound)
        return;
    const auto sample = effect.get<SoundRequest>().id;
    if (restart_pending_ && sample == 182)
        return; // The restart farewell replaces the ordinary lost-ball voice.
    SoundRequest request = effect.get<SoundRequest>();
    if (corrections_.physical_sound && effect.slot < simulation_.balls.size())
        corrections_.physical_sound(request, simulation_.balls[effect.slot]);
    scoring_.sound(request);
    if (sample == 162 || sample == 156 || sample == 157 || sample == 164)
        ++interactions_;
    if (sample == 158 && effect.slot < 6 && simulation_.balls[effect.slot].motion_type() == physics::ball_type::launcher)
        ++launches_;
    if (sample == 150)
        ++drains_;
}
void PinballGame::on_effect(const rules::SessionEffect& effect, SimulationState&, rules::TurnState&, rules::SharedGameState&) {
    if (effect.kind() == rules::SessionEffect::Kind::stop_sounds) {
        auto r = effect.get<SoundStopRequest>();
        scoring_.stop_sounds(r.id, r.channel, r.group);
    }
    if (effect.kind() == rules::SessionEffect::Kind::sound)
        scoring_.sound(effect.get<SoundRequest>());
    if (effect.kind() == rules::SessionEffect::Kind::rule_display && effect.get<RuleDisplayRequest>().selector == 58)
        display_.warning(game_state_.turn.tilted ? -1 : game_state_.turn.warnings_left, unsigned(effect.get<RuleDisplayRequest>().ticks));
}
std::string PinballGame::status() const {
    if (restart_pending_)
        return restart_drained() ? "Press S to start" : "Draining";
    const auto turn = player_count() > 1 ? "Player " + std::to_string(current_player() + 1) + " | " : "";
    if (phase_ == Phase::ready)
        return "Press S to start";
    if (phase_ == Phase::game_over)
        return "Game over | S: new game";
    if (phase_ == Phase::ending) {
        if (game_state_.turn.phase() >= rules::GamePhase::match)
            return "Match | End of game";
        if (display_.record_page_active(rules::RecordPage::initials))
            return "Initials | flippers: letter | Enter: select | C: back";
        return "Records | Enter: continue";
    }
    if (phase_ == Phase::between_balls)
        return turn + "Bonus | Enter or both flippers to skip";
    if (game_state_.turn.phase() == rules::GamePhase::begin_ball || game_state_.turn.phase() == rules::GamePhase::serve_ball)
        return turn + "Serving ball " + std::to_string(ball_number_);
    if (game_state_.turn.showdown_phase == 2 && game_state_.turn.showdown_stage >= 4)
        return "Ultimate Showdown complete";
    if (game_state_.turn.tilted)
        return "TILT";
    if (game_state_.turn.showdown_intro)
        return "Ultimate Showdown | All switches score";
    if (game_state_.turn.showdown_phase == 2) {
        if (game_state_.turn.showdown_stage == 3)
            return "Ultimate Showdown | Shoot the centre hole";
        if (game_state_.turn.showdown_stage == 1 || game_state_.turn.showdown_stage == 2)
            return "Ultimate Showdown | Shoot the lit orbit";
        return "Ultimate Showdown | Shoot ramps, loops and holes";
    }
    if (game_state_.turn.lock_choice_timer)
        return "Pick a frenzy | Flippers: choose | Enter: confirm";
    if (game_state_.turn.video_active)
        return "Video mode | Flippers: fire | Both: skip instructions";
    if (game_state_.turn.explosion_wait)
        return "Explosion award";
    if (game_state_.turn.flipper_timing_window)
        return "Press both flippers for the timing award";
    if (game_state_.turn.launch_enabled) {
        for (const auto& ball : simulation_.balls)
            if (ball.active() == 1 && ball.motion_type() == physics::ball_type::launcher && ball.transition_kind() == 16)
                return turn + "Ball " + std::to_string(ball_number_) + " / 3 | Enter: launch";
        return turn + "Serving ball " + std::to_string(ball_number_);
    }
    return turn + "Ball " + std::to_string(ball_number_) + " / 3";
}
unsigned SimulationClock::advance(std::uint64_t nanoseconds) {
    // Cap catch-up at 250ms so moving/resizing the window cannot cause a huge burst.
    constexpr std::uint64_t maximum_catchup_ns = nanoseconds_per_second / 4;
    remainder_ += std::min(nanoseconds, maximum_catchup_ns) * frames_per_second;
    auto frames = unsigned(remainder_ / nanoseconds_per_second);
    remainder_ %= nanoseconds_per_second;
    return frames;
}
} // namespace web
