#pragma once
#include "session_rules.hpp"

namespace web::rules {
class MissionRules;
class UltimateRules;
// Confirmed insert roles used by the recovered award controllers. Aliases
// retain original shared insert ownership instead of inventing new outputs.
enum class TableLamp : unsigned {
    extra_ball_award = 0,
    combo = 11,
    power_level = 12,
    mission_start = 13,
    kickback = 14,
    video_mode = 28,
    lock_ready = 29,
    quick_shot = 34,
    super_launcher = quick_shot,
    explosion = 37,
    station_magnetic_hole = 38,
    station_dock = 43,
    magnetic_ready = 51,
    mission_ready = 53,
    extra_ball_lit = 54,
    drop_target_first = 56
};
class LampRules {
public:
    static constexpr std::size_t count = 79;
    void initialize(TurnState&, SharedGameState&, LampState&) const;                      // 0x404a34
    void clear_queue(TurnState&, SharedGameState&) const;                                 // 0x404b08
    void queue(TurnState&, SharedGameState&, unsigned lamp, std::int32_t duration) const; // 0x4056d8
    void queue(TurnState& rules, SharedGameState& session, TableLamp lamp, std::int32_t duration) const {
        queue(rules, session, unsigned(lamp), duration);
    }
    void step_queue(TurnState&, SharedGameState&, LampState&) const;                                        // 0x405704
    void blink(const TurnState&, SharedGameState&, LampState&, unsigned lamp, std::int32_t duration) const; // 0x405764
    void begin_animation(TurnState&, SharedGameState&, std::int32_t mode) const;                            // 0x405820
    void step_animation(TurnState&, SharedGameState&) const;                                                // 0x405884
    // Original 0x405ba8. Some presentation arms also update shared original
    // counters/flags; their order is part of the recovered rules contract.
    void compose_table(const physics::SimulationState&, TurnState&, SharedGameState&, LampState&, MissionRules* missions = nullptr, UltimateRules* ultimate = nullptr) const;
    void compose_quick_shot(const TurnState&, SharedGameState&, LampState&) const; // 0x4098bc
    static std::int32_t value(const SharedGameState&, const LampState&, unsigned lamp);
    static void value(SharedGameState&, LampState&, unsigned lamp, std::int32_t value);
    static std::array<std::int32_t, count> values(const SharedGameState&, const LampState&);

private:
    void compose_award_inserts(const physics::SimulationState&, TurnState&, SharedGameState&, LampState&, Player&, UltimateRules*) const;
    void compose_objective_inserts(TurnState&, SharedGameState&, LampState&, Player&) const;
    void compose_shot_inserts(TurnState&, SharedGameState&, LampState&, Player&) const;
    void compose_control_inserts(TurnState&, SharedGameState&, LampState&, Player&) const;
    void compose_mode_inserts(TurnState&, SharedGameState&, LampState&, Player&, MissionRules*) const;
};
} // namespace web::rules
