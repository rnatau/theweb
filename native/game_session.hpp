#pragma once
#include "rules/missions.hpp"
#include "rules/score_records.hpp"
#include "render/dmd_display.hpp"
#include "music.hpp"
#include "render/table_state.hpp"
#include "input_timeline.hpp"
#include <functional>

namespace web {
// Host contract. Implementations own rules, physics and their synchronous
// callbacks; the window owns pacing, devices and persistence. State accessors
// expose read-only rendering/diagnostic views, never mutation hooks.
class GameSession {
public:
    enum class Phase {
        ready,
        playing,
        between_balls,
        game_over,
        ending
    };
    virtual ~GameSession() = default;
    virtual void initialize_engine_startup() = 0;
    virtual void prepare_exit_display() = 0;
    virtual unsigned startup_ticks() const = 0;
    virtual rules::ScoreValue score() const = 0;
    virtual unsigned current_player() const = 0;
    virtual unsigned player_count() const = 0;
    virtual const DotMatrixFrame& dot_matrix_frame() const = 0;
    virtual bool begin_presentation(const std::function<void(std::uint32_t)>& nudge_display = {}) = 0;
    virtual bool take_lamp_redraw() = 0;
    virtual bool lamp_animation_active() const = 0;
    virtual void show_effects_volume(unsigned) = 0;
    virtual void queue_effects_volume(unsigned) = 0;
    virtual void sound_sink(SoundSink*) = 0;
    virtual void music_restart_policy(unsigned) = 0;
    virtual MusicPlayback music_playback() const = 0;
    virtual void music_completed(MusicPlayback) = 0;
    virtual void load_scores(const Scores&) = 0;
    virtual Scores save_scores() const = 0;
    virtual void step(std::uint32_t actions) = 0;
    virtual void step_timed(const TimedInputFrame&) = 0;
    // Resumable Enhanced tick. Rules precede its fixed physics count; the last
    // complete microstep finishes the ordinary display/game-tick work.
    virtual unsigned begin_tick(const TimedInputFrame& rule_input) = 0;
    virtual bool step_microstep(TimedInputSample) = 0;
    virtual unsigned remaining_microsteps() const = 0;
    virtual void start() = 0;
    virtual bool restart_pending() const = 0;
    virtual bool restart_ready() const = 0;
    virtual TableFrameView table_view() const = 0;
    virtual unsigned physics_microsteps() const = 0;
    virtual const physics::SimulationState& simulation() const = 0;
    virtual const rules::TurnState& rule_state() const = 0;
    virtual std::array<std::int32_t, rules::LampRules::count> lamps() const = 0;
    virtual Phase phase() const = 0;
    virtual unsigned ball_number() const = 0;
    virtual std::uint64_t frames() const = 0;
    virtual std::uint64_t launches() const = 0;
    virtual std::uint64_t drains() const = 0;
    virtual std::uint64_t interactions() const = 0;
    virtual std::uint64_t rejected_starts() const = 0;
    virtual std::string status() const = 0;
};
} // namespace web
