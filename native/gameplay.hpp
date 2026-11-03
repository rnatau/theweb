#pragma once
#include "game_session.hpp"
#include "game_state.hpp"
#include "game_corrections.hpp"
#include "simulation_clock.hpp"
#include "rules/drop_targets.hpp"
#include "rules/table_mechanisms.hpp"
#include "rules/scoring.hpp"
#include "rules/score_records.hpp"
#include "rules/missions.hpp"
#include "rules/shot_sequences.hpp"
#include "rules/bonus.hpp"
#include "rules/centre_shot.hpp"
#include "rules/collection_policy.hpp"
#include "rules/loop_combos.hpp"
#include "render/dmd_display.hpp"
#include "music.hpp"
#include <functional>
#include <optional>

namespace web {
namespace tests {
struct GameAccess;
}
// Recovered local player turns, physics, physical reactions, launch/save,
// scoring, bonus, missions and end-game records/match. Multiball modes use the
// original ball requests and physics. Typed display/sound requests and music
// selections connect to independent presentation and optional playback owners.
class PinballGame final : public GameSession, private physics::PhysicsEventSink, private rules::BallRuleEffectSink, private rules::SessionEffectSink {
public:
    // Post-poll timing is only for explicitly controlled subsystem fixtures.
    enum class InputTiming {
        original,
        post_poll_fixture,
        timestamped
    };
    using RestartGesture = std::function<bool(std::uint32_t, const rules::TurnState&, int)>;
    explicit PinballGame(
        const std::filesystem::path& assets,
        InputTiming timing = InputTiming::original,
        rules::StartAction start_action = nullptr,
        RestartGesture restart_gesture = {},
        rules::CollectionPolicy collection_policy = nullptr,
        GameCorrections corrections = {});
    void initialize_engine_startup() override; // Original program entry, before the first rules update.
    void startup_version(std::string_view version) {
        display_.startup_version(version);
    }
    void prepare_exit_display() override {
        display_.prepare_exit(records_.ordinary().front());
    }
    unsigned startup_ticks() const override {
        return game_state_.shared.dmd_status == rules::dmd_status::startup ? unsigned(game_state_.turn.startup_display_ticks) : 0;
    }
    PinballGame(const PinballGame&) = delete;
    PinballGame& operator=(const PinballGame&) = delete;
    rules::ScoreValue score() const override {
        return scoring_.score();
    }
    rules::MissionStatus mission_status() const {
        return missions_.status();
    }
    unsigned bonus_multiplier() const {
        return unsigned(game_state_.shared.players().at(current_player()).bonus.multiplier);
    }
    unsigned current_player() const override {
        return unsigned(game_state_.turn.current_player);
    }
    unsigned player_count() const override {
        return unsigned(game_state_.turn.player_count);
    }
    const Bitmap& dot_matrix_image() const {
        return display_.image();
    }
    const DotMatrixFrame& dot_matrix_frame() const override {
        return display_.frame();
    }
    void start() override;
    void step_timed(const TimedInputFrame&) override;
    unsigned begin_tick(const TimedInputFrame&) override;
    bool step_microstep(TimedInputSample) override;
    unsigned remaining_microsteps() const override {
        return tick_microsteps_ - microstep_index_;
    }
    bool restart_pending() const override {
        return restart_pending_;
    }
    bool restart_ready() const override;
    bool begin_presentation(const std::function<void(std::uint32_t)>& nudge_display = {}) override; // Preceding nudge/start edges.
    bool take_lamp_redraw() override {
        const bool redraw = serve_state_.redraw_lamps != 0;
        serve_state_.redraw_lamps = 0;
        return redraw;
    }
    bool lamp_animation_active() const override {
        return game_state_.shared.lamp_animation_enabled != 0;
    }
    void show_effects_volume(unsigned value) override {
        display_.effects_volume(value);
    }
    void queue_effects_volume(unsigned value) override {
        pending_effects_volume_ = value;
    }
    void sound_sink(SoundSink* sink) override {
        scoring_.sound_sink(sink);
        extra_balls_.sound_sink(sink);
        game_end_.sound_sink(sink);
    }
    void music_restart_policy(unsigned policy) override {
        if (policy > 2)
            throw std::out_of_range("Unknown original music restart policy");
        music_restart_policy_ = policy;
    }
    MusicPlayback music_playback() const override {
        return {game_state_.shared.music_track, game_state_.shared.music_revision(), game_state_.turn.music_fade_timer};
    }
    void music_completed(MusicPlayback playback) override {
        if (game_state_.shared.music_revision() == playback.revision && game_state_.shared.music_track == playback.track)
            game_state_.shared.request_music_track(-1);
    }
    void load_scores(const Scores& scores) override {
        if (phase_ != Phase::ready)
            throw std::logic_error("Scores can only be loaded before playing");
        records_.load(scores, scoring_.replay());
    }
    Scores save_scores() const override {
        return records_.save(scoring_.replay());
    }
    void step(std::uint32_t actions) override;
    TableFrameView table_view() const override {
        return {simulation_, game_state_.shared.lamps.outputs, dot_matrix_frame(), game_state_.turn.live_balls, lamp_animation_active()};
    }
    unsigned physics_microsteps() const override {
        return physics::PhysicsSimulation::microstep_count(simulation_.refresh_rate, simulation_.double_steps);
    }
    const physics::SimulationState& simulation() const override {
        return simulation_;
    }
    const rules::TurnState& rule_state() const override {
        return game_state_.turn;
    }
    std::array<std::int32_t, rules::LampRules::count> lamps() const override {
        return rules::LampRules::values(game_state_.shared, game_state_.shared.lamps);
    }
    Phase phase() const override {
        return phase_;
    }
    unsigned ball_number() const override {
        return ball_number_;
    }
    std::uint64_t frames() const override {
        return frames_;
    }
    std::uint64_t interactions() const override {
        return interactions_;
    }
    std::uint64_t drains() const override {
        return drains_;
    }
    std::uint64_t launches() const override {
        return launches_;
    }
    std::string status() const override;
    std::uint64_t rejected_starts() const override {
        return rejected_starts_;
    }

private:
    friend struct tests::GameAccess;
    GameCorrections corrections_;
    physics::PhysicsSimulation physics_;
    physics::SimulationState simulation_;
    GameState game_state_;
    rules::BallLifecycle lifecycle_{corrections_.launcher};
    rules::SessionRules session_rules_;
    rules::LampRules lamp_rules_;
    DmdDisplay display_;
    rules::ServeState serve_state_;
    rules::ServeRules serve_{game_state_.turn, game_state_.shared, simulation_, serve_state_, *this, &display_, &display_};
    rules::ScoringRules scoring_{game_state_.turn, game_state_.shared, &display_};
    rules::ScoreRecords records_;
    rules::SpaceStationRules station_{game_state_.turn, game_state_.shared, simulation_, scoring_, &display_, &display_};
    rules::PowerLevelRules power_levels_{game_state_.turn, game_state_.shared, scoring_, &display_};
    rules::ExtraBallRules extra_balls_{game_state_.turn, game_state_.shared, [this] { display_.animation(34, DmdAnimationPlayer::replace); }};
    rules::VideoModeState video_state_;
    rules::VideoModeRules video_{game_state_.turn, game_state_.shared, scoring_, extra_balls_, video_state_, simulation_, &display_, corrections_.video_queue};
    rules::MissionState mission_state_;
    rules::MissionRules
        missions_{game_state_.turn, game_state_.shared, game_state_.shared.lamps, scoring_, simulation_.random, mission_state_, &extra_balls_, &display_, corrections_.mission_selection};
    rules::CentreShotRules centre_shot_{game_state_.turn, game_state_.shared, scoring_, extra_balls_, missions_, [this](rules::CentreRequest request) { on_centre_request(request); }};
    rules::ShotSequenceRules shot_sequences_{game_state_.turn, game_state_.shared, scoring_, &power_levels_};
    rules::DropTargetRules drop_targets_{&scoring_, [this] { station_.ready(); }};
    rules::TableMechanismRules
        table_mechanisms_{&scoring_, &extra_balls_, [this](unsigned id, std::uint32_t flags) { display_.animation(id, flags); }, [this] { explosion_.step(); }, [this] { station_.ramp_jackpot(); }};
    rules::BonusDisplayState bonus_display_;
    rules::LockState lock_state_;
    rules::LockAwards lock_awards_{game_state_.turn, game_state_.shared, scoring_, bonus_display_, lock_state_, &display_, &display_};
    rules::UltimateState ultimate_state_;
    rules::UltimateRules ultimate_{game_state_.turn, game_state_.shared, simulation_, scoring_, ultimate_state_, lock_awards_, &display_, &display_};
    rules::LoopComboRules loop_combos_{game_state_.turn, game_state_.shared, scoring_, bonus_display_, &display_};
    rules::SingleBallAwards single_ball_awards_{game_state_.turn, game_state_.shared, scoring_, loop_combos_, bonus_display_, &display_};
    rules::RampAwardRules ramp_awards_{game_state_.turn, game_state_.shared, scoring_, bonus_display_, &display_};
    rules::ExplosionState explosion_state_;
    rules::ExplosionRules
        explosion_{game_state_.turn, game_state_.shared, simulation_, scoring_, power_levels_, ramp_awards_, extra_balls_, video_, lock_awards_, explosion_state_, &display_, &display_};
    rules::BonusRules bonus_{game_state_.turn, game_state_.shared, scoring_, bonus_display_};
    rules::GameEndState end_state_;
    rules::GameEndRules
        game_end_{game_state_.turn, game_state_.shared, simulation_, records_, scoring_.replay(), explosion_state_.replay_delay, end_state_, &display_, &display_, corrections_.improved_records};
    rules::ActionRepeater input_repeat_;
    Phase phase_ = Phase::ready;
    unsigned ball_number_ = 0;
    std::uint32_t previous_actions_ = 0;
    unsigned music_restart_policy_ = 1;
    std::optional<unsigned> pending_effects_volume_;
    std::uint32_t pending_nudge_feedback_ = 0;
    bool original_input_order_ = true, pending_start_ = false;
    bool timestamped_input_ = false;
    rules::StartAction start_action_ = nullptr;
    std::uint64_t rejected_starts_ = 0;
    RestartGesture restart_gesture_;
    rules::CollectionPolicy collection_policy_ = nullptr;
    bool restart_pending_ = false;
    unsigned restart_frames_ = 0;
    std::uint64_t frames_ = 0, interactions_ = 0, drains_ = 0, launches_ = 0;
    void begin_ball();
    void drain_for_restart();
    void step_frame(std::uint32_t actions, const TimedInputFrame* input);
    void begin_frame(std::uint32_t actions, const TimedInputFrame* input);
    void finish_frame(std::uint32_t actions, std::uint32_t display_edges);
    void apply_physical_input(TimedInputSample, bool first);
    unsigned tick_microsteps_ = 0, microstep_index_ = 0;
    std::uint32_t frame_actions_ = 0, frame_edges_ = 0, previous_physical_actions_ = 0;
    void step_rules(std::uint32_t actions, const TimedInputFrame* timed = nullptr);
    void step_active_play(std::uint32_t& edges, std::uint32_t actions);
    // Snapshot completed rules/physics state for the independent DMD cadence.
    void update_display_contexts(std::uint32_t actions, std::uint32_t display_edges);
    void timers_and_serve(std::uint32_t edges);
    void advance_missions();
    bool collection_allowed() const;
    void compose_lamps();
    void resume_music();
    void on_centre_request(rules::CentreRequest request);
    void on_event(const physics::PhysicsEvent& event, physics::BallSlots& balls) override;
    void on_effect(const rules::BallRuleEffect& effect, physics::SimulationState&, rules::TurnState&) override;
    void on_effect(const rules::SessionEffect&, physics::SimulationState&, rules::TurnState&, rules::SharedGameState&) override;
};
} // namespace web
