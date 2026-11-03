#pragma once
#include "ball_rules.hpp"
#include "lamp_state.hpp"

namespace web::rules {
class SharedGameState {
public:
    static constexpr unsigned lamp_queue_capacity = 8;
    explicit SharedGameState(PlayerRoster& players) :
        players_(players) {}
    void reset();
    PlayerRoster& players() {
        return players_;
    }
    const PlayerRoster& players() const {
        return players_;
    }
    LampState lamps;
    std::array<LampFlash, lamp_queue_capacity> lamp_queue;
    std::int32_t lamp_animation_enabled = 0;
    std::int32_t music_track = 0;
    std::int32_t dmd_queue_aux = 0;
    std::int32_t web_ramp_combo_timer = 0;
    std::int32_t jet_gate_timer = 0;
    std::int32_t skyscraper_extended = 0;
    std::int32_t right_combo_entry_timer = 0;
    std::int32_t ammo_strength = 0;
    std::int32_t shuttle_charge = 0;
    std::int32_t right_loop_lamp_timer = 0;
    std::int32_t hunt_remaining = 0;
    std::int32_t jet_completion_window = 0;
    std::int32_t left_loop_lamp_timer = 0;
    std::int32_t mission_centre_lit = 0;
    std::int32_t reactor_remaining = 0;
    std::int32_t ammo_stage = 0;
    std::int32_t reactor_regeneration = 0;
    std::int32_t ammo_remaining = 0;
    std::int32_t shuttle_hits = 0;
    std::int32_t combo_mania_timer = 0;
    std::int32_t combo_count = 0;
    std::int32_t hunt_target = 0;
    std::int32_t mission_kind = 0;
    std::int32_t combo_remaining = 0;
    std::int32_t left_combo_entry_timer = 0;
    std::int32_t skyscraper_cooldown = 0;
    std::int32_t skyscraper_reset = 0;
    std::int32_t mode_reset = 0;
    std::int32_t left_spinner_timer = 0;
    std::int32_t station_super_holdoff = 0;
    std::int32_t station_super_window = 0;
    std::int32_t right_loop_completed = 0;
    std::int32_t right_loop_completion_timer = 0;
    std::int32_t initials_initialized = 0;
    std::int32_t loop_count = 0;
    std::int32_t initials_letters = 0;
    std::int32_t dmd_queue_state = 0;
    std::int32_t dmd_queue_count = 0;
    std::int32_t dmd_frame = 0;
    std::int32_t warning_display = 0;
    std::int32_t dmd_animation = 0;
    std::int32_t dmd_aux = 0;
    std::int32_t dmd_status = 0;
    std::span<std::uint8_t> lamp_cells() {
        return lamps.animation_cells;
    }
    std::uint32_t lamp_index(std::size_t index) const {
        return lamps.sample_indices.at(index);
    }
    void lamp_index(std::size_t index, std::uint32_t value) {
        lamps.sample_indices.at(index) = value;
    }
    std::int32_t lamp_value(std::size_t index) const {
        return lamps.outputs.at(index);
    }
    void lamp_value(std::size_t index, std::int32_t value) {
        lamps.outputs.at(index) = value;
    }
    std::uint64_t music_revision() const {
        return music_revision_;
    }
    void request_music_track(std::int32_t value) {
        music_track = value;
        ++music_revision_;
    }

private:
    PlayerRoster& players_;
    std::uint64_t music_revision_ = 0;
};
using SessionMember = std::int32_t SharedGameState::*;

namespace session_field {
inline constexpr auto web_ramp_combo_timer = &SharedGameState::web_ramp_combo_timer;
inline constexpr auto jet_gate_timer = &SharedGameState::jet_gate_timer;
inline constexpr auto right_combo_entry_timer = &SharedGameState::right_combo_entry_timer;
inline constexpr auto right_loop_lamp_timer = &SharedGameState::right_loop_lamp_timer;
inline constexpr auto jet_completion_window = &SharedGameState::jet_completion_window;
inline constexpr auto left_loop_lamp_timer = &SharedGameState::left_loop_lamp_timer;
inline constexpr auto combo_mania_timer = &SharedGameState::combo_mania_timer;
inline constexpr auto combo_count = &SharedGameState::combo_count;
inline constexpr auto combo_remaining = &SharedGameState::combo_remaining;
inline constexpr auto left_combo_entry_timer = &SharedGameState::left_combo_entry_timer;
inline constexpr auto left_spinner_timer = &SharedGameState::left_spinner_timer;
inline constexpr auto station_super_holdoff = &SharedGameState::station_super_holdoff;
inline constexpr auto station_super_window = &SharedGameState::station_super_window;
inline constexpr auto right_loop_completed = &SharedGameState::right_loop_completed;
inline constexpr auto right_loop_completion_timer = &SharedGameState::right_loop_completion_timer;
inline constexpr auto initials_initialized = &SharedGameState::initials_initialized;
inline constexpr auto loop_count = &SharedGameState::loop_count;
inline constexpr auto initials_letters = &SharedGameState::initials_letters;
inline constexpr auto lamp_animation_enabled = &SharedGameState::lamp_animation_enabled;
inline constexpr auto warning_display = &SharedGameState::warning_display;
inline constexpr auto music_state = &SharedGameState::music_track;
inline constexpr auto dmd_queue_count = &SharedGameState::dmd_queue_count;
inline constexpr auto dmd_queue_state = &SharedGameState::dmd_queue_state;
inline constexpr auto dmd_queue_aux = &SharedGameState::dmd_queue_aux;
inline constexpr auto dmd_animation = &SharedGameState::dmd_animation;
inline constexpr auto dmd_frame = &SharedGameState::dmd_frame;
inline constexpr auto dmd_status = &SharedGameState::dmd_status;
inline constexpr auto dmd_aux = &SharedGameState::dmd_aux;
inline constexpr auto mission_kind = &SharedGameState::mission_kind;
inline constexpr auto mission_centre_lit = &SharedGameState::mission_centre_lit;
inline constexpr auto reactor_remaining = &SharedGameState::reactor_remaining;
inline constexpr auto reactor_regeneration = &SharedGameState::reactor_regeneration;
inline constexpr auto hunt_remaining = &SharedGameState::hunt_remaining;
inline constexpr auto hunt_target = &SharedGameState::hunt_target;
inline constexpr auto ammo_remaining = &SharedGameState::ammo_remaining;
inline constexpr auto ammo_stage = &SharedGameState::ammo_stage;
inline constexpr auto ammo_strength = &SharedGameState::ammo_strength;
inline constexpr auto shuttle_hits = &SharedGameState::shuttle_hits;
inline constexpr auto shuttle_charge = &SharedGameState::shuttle_charge;
inline constexpr auto skyscraper_cooldown = &SharedGameState::skyscraper_cooldown;
inline constexpr auto skyscraper_extended = &SharedGameState::skyscraper_extended;
} // namespace session_field
enum class SessionEffectKind : std::uint32_t {
    sound = 1,
    rule_display,
    music_command,
    stop_sounds
};
struct SessionEffect : TypedEffect<SessionEffectKind, SoundRequest, RuleDisplayRequest, MusicStopRequest, SoundStopRequest> {
    using TypedEffect::TypedEffect;
};
class SessionEffectSink {
public:
    virtual ~SessionEffectSink() = default;
    virtual void on_effect(const SessionEffect& effect, physics::SimulationState& simulation, TurnState& rules, SharedGameState& session) = 0;
};
class SessionRules {
public:
    // Native host metadata, not an original return register. Report the branch
    // actually taken so presentation never has to duplicate start eligibility.
    enum class StartResult {
        ignored,
        player_added,
        restarted,
        rejected
    };
    void initialize_player(TurnState& rules, SharedGameState& session, std::size_t player) const;                                          // 0x404cbc
    void setup_ball(physics::SimulationState& simulation, TurnState& rules, SharedGameState& session) const;                               // 0x404ed4
    void sample_lamps(SharedGameState& session) const;                                                                                     // 0x4057cc
    void clear_lamp_animation(TurnState& rules, SharedGameState& session) const;                                                           // 0x4057f8
    void announce(physics::SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects) const;     // 0x410bd0
    void tilt(physics::SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects) const;         // 0x412758
    StartResult start(physics::SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects) const; // 0x4053f0
private:
    void stop_music(physics::SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects) const;
};
using StartAction = SessionRules::StartResult (*)(physics::SimulationState&, TurnState&, SharedGameState&, SessionEffectSink&);
// Install in the remaining-handler position of BallRuleCallbacks. Recovered tilt
// effects execute here; spinner scoring and any future requests keep forwarding.
class SessionRuleCallbacks : public physics::PhysicsEventSink {
public:
    SessionRuleCallbacks(physics::SimulationState& simulation, TurnState& rules, SharedGameState& session, SessionEffectSink& effects, physics::PhysicsEventSink& remaining) :
        simulation_(simulation),
        rules_(rules),
        session_(session),
        effects_(effects),
        remaining_(remaining) {}
    void on_event(const physics::PhysicsEvent& event, physics::BallSlots& balls) override;

private:
    physics::SimulationState& simulation_;
    TurnState& rules_;
    SharedGameState& session_;
    SessionEffectSink& effects_;
    physics::PhysicsEventSink& remaining_;
};
} // namespace web::rules
