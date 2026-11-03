#pragma once
#include "../physics/simulation.hpp"
#include "rule_time.hpp"
#include "state_codes.hpp"
#include "player.hpp"
#include <optional>

namespace web::rules {
// Names describe confirmed contact roles; A/B/C retain original ordering where
// a more specific physical label has not been recovered.
enum class TableSwitch : std::size_t {
    spinner_b_lane_entry = 2,
    spinner_a_lane_entry = 4,
    sinkhole_b = 7,
    sinkhole_a = 8,
    spinner_b = 9,
    sling_a = 10,
    sling_b = 11,
    spinner_a = 12,
    ramp_a_entry = 15,
    web_ramp_entry = 16,
    ramp_a = 17,
    web_ramp = 18,
    centre_shot = 19,
    centre_sinkhole = 20,
    loop_entry = 21,
    loop_middle = 22,
    loop_gate_a = 23,
    loop_gate_b = 24,
    drop_target_first = 25, // Six consecutive target contacts, 25..30.
    upper_sinkhole = 31,
    bumper_a = 32,
    bumper_b = 33,
    bumper_c = 34,
    jet_sequence_gate = 40
};
// Stored stage codes, not bit flags. The original skips value three.
enum class StationSuperStage : std::int32_t {
    inactive = 0,
    targets_required = 1,
    magnetic_hole_required = 2,
    ramp_collection_window = 4
};
// Readiness codes shared by the original lock, station and award controllers.
// These remain integers because the recovered state store also preserves aliases.
namespace mode_state {
inline constexpr std::int32_t inactive = 0, ready = 1, active = 2;
}
// Active-turn rules. Storage is native C++ state; legacy snapshots are adapters.
class TurnState {
public:
    explicit TurnState(PlayerRoster& players) :
        players_(players) {}
    PlayerRoster& players() {
        return players_;
    }
    const PlayerRoster& players() const {
        return players_;
    }
    std::int32_t mission_time = 0;
    std::int32_t kickback_rearm_timer = 0;
    std::int32_t loop_remaining = 0;
    std::int32_t station_centre_stage = 0;
    std::int32_t left_loop_completed = 0;
    std::int32_t reserved_0 = 0;
    std::int32_t station_stage_code = 0;
    std::int32_t magnet_voice_timer = 0;
    std::int32_t station_ramp_ready = 0;
    std::int32_t left_loop_completion_timer = 0;
    std::int32_t right_spinner_timer = 0;
    std::int32_t magnet_combo_timer = 0;
    std::int32_t inlane_a_timer = 0;
    std::int32_t magnet_activation_window = 0;
    std::int32_t left_spinner_lane_timer = 0;
    std::int32_t inlane_b_timer = 0;
    std::int32_t station_triple_ready = 0;
    std::int32_t reserved_1 = 0;
    std::int32_t right_spinner_lane_timer = 0;
    std::int32_t web_ramp_entry_timer = 0;
    std::int32_t mission_started_this_frame = 0;
    std::int32_t announce_pending = 0;
    std::int32_t save_timer = 0;
    std::int32_t loop_completed = 0;
    std::int32_t auto_launch_block = 0;
    std::int32_t auto_kick_enabled = 0;
    std::int32_t quick_shot_value = 0;
    std::int32_t quick_shot_timer = 0;
    std::int32_t player_cycle = 0;
    std::int32_t serve_spacing_timer = 0;
    std::int32_t return_toggle = 0;
    std::int32_t loop_first_gate_timer = 0;
    std::int32_t super_rescue = 0;
    std::int32_t quick_shot_warning = 0;
    std::int32_t serve_requests = 0;
    std::int32_t diverter_enabled = 0;
    std::int32_t diverter_warning = 0;
    std::int32_t extra_ball_turn = 0;
    std::int32_t loop_last_gate_timer = 0;
    std::int32_t extra_ball_diverter = 0;
    std::int32_t super_jackpot = 0;
    std::int32_t quick_shot_side = 0;
    std::int32_t station_super_lit = 0;
    std::int32_t ramp_a_entry_timer = 0;
    std::int32_t showdown_hits = 0;
    std::int32_t positive_sling_inhibit = 0;
    std::int32_t negative_sling_inhibit = 0;
    std::int32_t lamp_queue = 0;
    std::int32_t centre_award_consumed = 0;
    std::int32_t announcement_voice = 0;
    std::int32_t target_bank_b_timer = 0;
    std::int32_t outlane_extra_ball = 0;
    std::int32_t mission_flash = 0;
    std::int32_t super_launcher_timer = 0;
    std::int32_t target_bank_a_timer = 0;
    std::int32_t super_launcher_hits = 0;
    std::int32_t super_launcher_countdown = 0;
    std::int32_t showdown_phase = 0;
    std::int32_t showdown_stage = 0;
    std::int32_t mission_flash_time = 0;
    std::int32_t mission_intro = 0;
    std::int32_t explosion_wait = 0;
    std::int32_t showdown_rotating_lamp = 0;
    std::int32_t drain_extension = 0;
    std::int32_t combined_lock = 0;
    std::int32_t phase_code = 0;
    std::int32_t warnings_left = 0;
    std::int32_t status_right_hold = 0;
    std::int32_t centre_award_wait = 0;
    std::int32_t lock_choice_timer = 0;
    std::int32_t status_selection = 0;
    std::int32_t tilt_lock = 0;
    std::int32_t status_timer = 0;
    std::int32_t launch_timer = 0;
    std::int32_t reserved_2 = 0;
    std::int32_t continue_timer = 0;
    std::int32_t elapsed = 0;
    std::int32_t tilted = 0;
    std::int32_t single_ball_award_mode = 0;
    std::int32_t reserved_3 = 0;
    std::int32_t bonus_stage = 0;
    std::int32_t reserved_4 = 0;
    std::int32_t lamp_frame = 0;
    std::int32_t status_left_hold = 0;
    std::int32_t reserved_5 = 0;
    std::int32_t lamp_mode = 0;
    std::int32_t record_scan_index = 0;
    std::int32_t showdown_artwork = 0;
    std::int32_t record_best_high = 0;
    std::int32_t record_rank = 0;
    std::int32_t record_best_low = 0;
    std::int32_t showdown_sound_timer = 0;
    std::int32_t record_best_player = 0;
    std::int32_t showdown_finish_timer = 0;
    std::int32_t initials_confirmation_timer = 0;
    std::int32_t match_timer = 0;
    std::int32_t record_maximum = 0;
    std::int32_t showdown_acceleration = 0;
    std::int32_t kickback_announcement_played = 0;
    std::int32_t lamp_animation = 0;
    std::int32_t reserved_6 = 0;
    std::int32_t showdown_sound_phase = 0;
    std::int32_t flipper_timing_window = 0;
    std::int32_t music_fade_timer = 0;
    std::int32_t award_retrigger_guard = 0;
    std::int32_t bonus_display_gate = 0;
    std::int32_t music_requested_track = 0;
    std::int32_t showdown_intro_timer = 0;
    std::int32_t match_awarded = 0;
    std::int32_t current_player = 0;
    std::int32_t lamp_previous_frame_phase = 0;
    std::int32_t start_latch = 0;
    std::int32_t player_turn = 0;
    std::int32_t video_active = 0;
    std::int32_t player_count = 0;
    std::int32_t launch_enabled = 0;
    std::int32_t returns_paused = 0;
    std::int32_t mission_total = 0;
    std::int32_t super_jet_total = 0;
    std::int32_t showdown_total = 0;
    std::int32_t startup_display_ticks = 0;
    std::int32_t video_lock_pending = 0;
    std::int32_t lock_choice = 0;
    std::int32_t showdown_reserves = 0;
    std::int32_t skyscraper_floor = 0;
    std::int32_t showdown_intro = 0;
    std::int32_t match_digit = 0;
    std::int32_t drain_override = 0;
    std::int32_t match_stage = 0;
    std::int32_t super_skillshot = 0;
    std::int32_t initials_cursor = 0;
    std::int32_t bonus_timer = 0;
    std::int32_t record_display_rank = 0;
    std::int32_t record_entry_player = 0;
    std::int32_t live_balls = 0;
    GamePhase phase() const {
        return GamePhase(phase_code);
    }
    void phase(GamePhase value) {
        phase_code = std::int32_t(value);
    }
    StationSuperStage station_super_stage() const {
        return StationSuperStage(station_stage_code);
    }
    void station_super_stage(StationSuperStage value) {
        station_stage_code = std::int32_t(value);
    }
    void switch_contact(std::int32_t);
    std::int32_t switch_word(std::size_t index) const;
    bool switch_edge(std::size_t index) const;
    bool switch_edge(TableSwitch contact) const {
        return switch_edge(std::size_t(contact));
    }
    void sample_switches();
    void clear_switch_edge(TableSwitch);
    std::int32_t player_relaunch(std::size_t player) const {
        return players_.at(unsigned(player)).ball.relaunch;
    }
    void player_relaunch(std::size_t player, std::int32_t value) {
        players_.at(unsigned(player)).ball.relaunch = value;
    }
    std::uint32_t frame_phase() const {
        return frame_phase_;
    }
    void frame_phase(std::uint32_t value) {
        frame_phase_ = value;
    }
    void reset();
    std::int32_t& previous_switch(std::size_t index);
    const std::int32_t& previous_switch(std::size_t index) const;
    std::int32_t& edge_switch(std::size_t index);
    const std::int32_t& edge_switch(std::size_t index) const;
    std::int32_t& contact_switch(std::size_t index);
    const std::int32_t& contact_switch(std::size_t index) const;

private:
    PlayerRoster& players_;
    std::array<std::int32_t, 43> previous_switches_{}, switch_edges_{};
    std::array<std::int32_t, 64> switch_contacts_{};
    std::uint32_t frame_phase_ = 0;
};
using TurnMember = std::int32_t TurnState::*;

namespace turn_field {
inline constexpr auto serve_spacing_timer = &TurnState::serve_spacing_timer;
inline constexpr auto station_centre_stage = &TurnState::station_centre_stage;
inline constexpr auto left_loop_completed = &TurnState::left_loop_completed;
inline constexpr auto station_super_stage = &TurnState::station_stage_code;
inline constexpr auto magnet_voice_timer = &TurnState::magnet_voice_timer;
inline constexpr auto station_ramp_ready = &TurnState::station_ramp_ready;
inline constexpr auto left_loop_completion_timer = &TurnState::left_loop_completion_timer;
inline constexpr auto right_spinner_timer = &TurnState::right_spinner_timer;
inline constexpr auto magnet_combo_timer = &TurnState::magnet_combo_timer;
inline constexpr auto inlane_a_timer = &TurnState::inlane_a_timer;
inline constexpr auto magnet_activation_window = &TurnState::magnet_activation_window;
inline constexpr auto left_spinner_lane_timer = &TurnState::left_spinner_lane_timer;
inline constexpr auto inlane_b_timer = &TurnState::inlane_b_timer;
inline constexpr auto station_triple_ready = &TurnState::station_triple_ready;
inline constexpr auto right_spinner_lane_timer = &TurnState::right_spinner_lane_timer;
inline constexpr auto loop_completed = &TurnState::loop_completed;
inline constexpr auto loop_first_gate_timer = &TurnState::loop_first_gate_timer;
inline constexpr auto super_rescue = &TurnState::super_rescue;
inline constexpr auto diverter_warning = &TurnState::diverter_warning;
inline constexpr auto loop_last_gate_timer = &TurnState::loop_last_gate_timer;
inline constexpr auto station_super_lit = &TurnState::station_super_lit;
inline constexpr auto loop_remaining = &TurnState::loop_remaining;
inline constexpr auto record_scan_index = &TurnState::record_scan_index;
inline constexpr auto record_best_high = &TurnState::record_best_high;
inline constexpr auto record_rank = &TurnState::record_rank;
inline constexpr auto record_best_low = &TurnState::record_best_low;
inline constexpr auto showdown_sound_timer = &TurnState::showdown_sound_timer;
inline constexpr auto record_best_player = &TurnState::record_best_player;
inline constexpr auto showdown_finish_timer = &TurnState::showdown_finish_timer;
inline constexpr auto initials_confirmation_timer = &TurnState::initials_confirmation_timer;
inline constexpr auto match_timer = &TurnState::match_timer;
inline constexpr auto record_maximum = &TurnState::record_maximum;
inline constexpr auto showdown_acceleration = &TurnState::showdown_acceleration;
inline constexpr auto kickback_announcement_played = &TurnState::kickback_announcement_played;
inline constexpr auto showdown_sound_phase = &TurnState::showdown_sound_phase;
inline constexpr auto music_fade_timer = &TurnState::music_fade_timer;
inline constexpr auto record_flow_stage = &TurnState::bonus_display_gate;
inline constexpr auto bonus_display_gate = &TurnState::bonus_display_gate;
inline constexpr auto music_requested_track = &TurnState::music_requested_track;
inline constexpr auto match_awarded = &TurnState::match_awarded;
inline constexpr auto lamp_previous_frame_phase = &TurnState::lamp_previous_frame_phase;
inline constexpr auto super_jet_total = &TurnState::super_jet_total;
inline constexpr auto match_digit = &TurnState::match_digit;
inline constexpr auto match_stage = &TurnState::match_stage;
inline constexpr auto loop_awarded = &TurnState::super_skillshot;
inline constexpr auto super_skillshot = &TurnState::super_skillshot;
inline constexpr auto initials_cursor = &TurnState::initials_cursor;
inline constexpr auto record_display_rank = &TurnState::record_display_rank;
inline constexpr auto record_entry_player = &TurnState::record_entry_player;
inline constexpr auto drain_gate = &TurnState::mission_time;
inline constexpr auto mission_time = &TurnState::mission_time;
inline constexpr auto mission_total = &TurnState::mission_total;
inline constexpr auto mission_flash = &TurnState::mission_flash;
inline constexpr auto mission_flash_time = &TurnState::mission_flash_time;
inline constexpr auto mission_intro = &TurnState::mission_intro;
inline constexpr auto mission_started_this_frame = &TurnState::mission_started_this_frame;
inline constexpr auto centre_award_consumed = &TurnState::centre_award_consumed;
inline constexpr auto centre_award_wait = &TurnState::centre_award_wait;
inline constexpr auto skyscraper_floor = &TurnState::skyscraper_floor;
inline constexpr auto save_timer = &TurnState::save_timer;
inline constexpr auto auto_launch_block = &TurnState::auto_launch_block;
inline constexpr auto auto_kick_enabled = &TurnState::auto_kick_enabled;
inline constexpr auto return_toggle = &TurnState::return_toggle;
inline constexpr auto serve_requests = &TurnState::serve_requests;
inline constexpr auto diverter_enabled = &TurnState::diverter_enabled;
inline constexpr auto positive_sling_inhibit = &TurnState::positive_sling_inhibit;
inline constexpr auto negative_sling_inhibit = &TurnState::negative_sling_inhibit;
inline constexpr auto tilt_lock = &TurnState::tilt_lock;
inline constexpr auto drain_extension = &TurnState::drain_extension;
inline constexpr auto phase = &TurnState::phase_code;
inline constexpr auto status_right_hold = &TurnState::status_right_hold;
inline constexpr auto status_left_hold = &TurnState::status_left_hold;
inline constexpr auto status_selection = &TurnState::status_selection;
inline constexpr auto status_timer = &TurnState::status_timer;
inline constexpr auto launch_timer = &TurnState::launch_timer;
inline constexpr auto elapsed = &TurnState::elapsed;
inline constexpr auto bonus_stage = &TurnState::bonus_stage;
inline constexpr auto bonus_timer = &TurnState::bonus_timer;
inline constexpr auto continue_timer = &TurnState::continue_timer;
inline constexpr auto extra_ball_turn = &TurnState::extra_ball_turn;
inline constexpr auto extra_ball_diverter = &TurnState::extra_ball_diverter;
inline constexpr auto outlane_extra_ball = &TurnState::outlane_extra_ball;
inline constexpr auto kickback_rearm_timer = &TurnState::kickback_rearm_timer;
inline constexpr auto web_ramp_entry_timer = &TurnState::web_ramp_entry_timer;
inline constexpr auto ramp_a_entry_timer = &TurnState::ramp_a_entry_timer;
inline constexpr auto drain_policy = &TurnState::single_ball_award_mode;
inline constexpr auto single_ball_award_mode = &TurnState::single_ball_award_mode;
inline constexpr auto current_player = &TurnState::current_player;
inline constexpr auto launch_enabled = &TurnState::launch_enabled;
inline constexpr auto video_active = &TurnState::video_active;
inline constexpr auto video_lock_pending = &TurnState::video_lock_pending;
inline constexpr auto lock_choice_timer = &TurnState::lock_choice_timer;
inline constexpr auto lock_choice = &TurnState::lock_choice;
inline constexpr auto explosion_wait = &TurnState::explosion_wait;
inline constexpr auto flipper_timing_window = &TurnState::flipper_timing_window;
inline constexpr auto super_jackpot = &TurnState::super_jackpot;
inline constexpr auto combined_lock = &TurnState::combined_lock;
inline constexpr auto award_retrigger_guard = &TurnState::award_retrigger_guard;
inline constexpr auto super_launcher_timer = &TurnState::super_launcher_timer;
inline constexpr auto super_launcher_hits = &TurnState::super_launcher_hits;
inline constexpr auto super_launcher_countdown = &TurnState::super_launcher_countdown;
inline constexpr auto quick_shot_value = &TurnState::quick_shot_value;
inline constexpr auto quick_shot_timer = &TurnState::quick_shot_timer;
inline constexpr auto quick_shot_warning = &TurnState::quick_shot_warning;
inline constexpr auto quick_shot_side = &TurnState::quick_shot_side;
inline constexpr auto tilt_gate = &TurnState::showdown_stage;
inline constexpr auto showdown_stage = &TurnState::showdown_stage;
inline constexpr auto showdown_phase = &TurnState::showdown_phase;
inline constexpr auto showdown_hits = &TurnState::showdown_hits;
inline constexpr auto showdown_artwork = &TurnState::showdown_artwork;
inline constexpr auto showdown_intro = &TurnState::showdown_intro;
inline constexpr auto showdown_intro_timer = &TurnState::showdown_intro_timer;
inline constexpr auto showdown_reserves = &TurnState::showdown_reserves;
inline constexpr auto showdown_total = &TurnState::showdown_total;
inline constexpr auto showdown_rotating_lamp = &TurnState::showdown_rotating_lamp;
inline constexpr auto warnings_left = &TurnState::warnings_left;
inline constexpr auto tilted = &TurnState::tilted;
inline constexpr auto lamp_animation = &TurnState::lamp_animation;
inline constexpr auto lamp_mode = &TurnState::lamp_mode;
inline constexpr auto lamp_frame = &TurnState::lamp_frame;
inline constexpr auto player_count = &TurnState::player_count;
inline constexpr auto start_latch = &TurnState::start_latch;
inline constexpr auto startup_display_ticks = &TurnState::startup_display_ticks;
inline constexpr auto player_turn = &TurnState::player_turn;
inline constexpr auto player_cycle = &TurnState::player_cycle;
inline constexpr auto announce_pending = &TurnState::announce_pending;
inline constexpr auto announcement_voice = &TurnState::announcement_voice;
inline constexpr auto lamp_queue = &TurnState::lamp_queue;
inline constexpr auto target_bank_a_timer = &TurnState::target_bank_a_timer;
inline constexpr auto target_bank_b_timer = &TurnState::target_bank_b_timer;
inline constexpr auto returns_paused = &TurnState::returns_paused;
inline constexpr auto drain_override = &TurnState::drain_override;
inline constexpr auto live_balls = &TurnState::live_balls;
} // namespace turn_field
struct StationContact {};
enum class BallEffectKind : std::uint32_t {
    sound = 1,
    display,
    station_contact
};
struct BallRuleEffect : TypedEffect<BallEffectKind, SoundRequest, AnimationRequest, StationContact> {
    std::size_t slot;
    template <class T>
    BallRuleEffect(std::size_t source, T value) :
        TypedEffect(value),
        slot(source) {}
};
class BallRuleEffectSink {
public:
    virtual ~BallRuleEffectSink() = default;
    virtual void on_effect(const BallRuleEffect& effect, physics::SimulationState& simulation, TurnState& rules) = 0;
};
class BallLifecycle {
public:
    using LauncherSelection = std::optional<std::size_t> (*)(const physics::BallSlots&);
    explicit BallLifecycle(LauncherSelection selection = nullptr) :
        selection_(selection) {}
    enum class Exit {
        right,
        left,
        lift
    };
    // Original 0x402214 selects the LAST slot whose active word is exactly zero.
    void serve(physics::SimulationState& simulation, TurnState& rules, BallRuleEffectSink& effects) const;
    // 0x402158: tests transition 16/type 2, not active or motion mode.
    void launch(physics::SimulationState& simulation, TurnState& rules, BallRuleEffectSink& effects) const;
    // Original 0x4021cc and return helpers 0x4022a8 / 0x402330 / 0x4023b4.
    void auto_launch(physics::SimulationState& simulation, TurnState& rules, std::size_t slot, BallRuleEffectSink& effects) const;
    void eject(physics::SimulationState& simulation, TurnState& rules, std::size_t slot, Exit exit, BallRuleEffectSink& effects) const;
    void begin_launch_window(TurnState& rules) const;                                                               // 0x412b2c
    void drained(physics::SimulationState& simulation, TurnState& rules, std::size_t slot) const;                   // 0x412b78
    void update_returns(physics::SimulationState& simulation, TurnState& rules, BallRuleEffectSink& effects) const; // 0x412914
    // Complete physical dispatcher 0x412d34. Space Station arm 11 emits the synchronous
    // 0x408d94 request; PinballGame connects its recovered Space Station owner.
    void surface_transition(physics::SimulationState& simulation, TurnState& rules, std::size_t slot, std::int32_t transition, BallRuleEffectSink& effects) const;

private:
    LauncherSelection selection_;
};
// Consume recovered drain/switch/surface boundaries. Other callbacks are
// forwarded synchronously; tilt/scoring and mission effects remain.
class BallRuleCallbacks : public physics::PhysicsEventSink {
public:
    BallRuleCallbacks(physics::SimulationState& simulation, TurnState& rules, BallRuleEffectSink& effects, physics::PhysicsEventSink& remaining) :
        simulation_(simulation),
        rules_(rules),
        effects_(effects),
        remaining_(remaining) {}
    void on_event(const physics::PhysicsEvent& event, physics::BallSlots& balls) override;

private:
    physics::SimulationState& simulation_;
    TurnState& rules_;
    BallRuleEffectSink& effects_;
    physics::PhysicsEventSink& remaining_;
    BallLifecycle lifecycle_;
};
} // namespace web::rules
