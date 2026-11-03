#pragma once
#include "lamps.hpp"
#include <functional>

namespace web::rules {
class ScoringRules;
// Six physical targets in two independently resetting banks of three. Storage
// stays in its existing owners: first word in SharedGameState, five in LampState.
class DropTargetRules {
public:
    // A null score boundary is used by the independent physical-rule fixtures.
    explicit DropTargetRules(ScoringRules* scoring = nullptr, std::function<void()> station_ready = {}) :
        scoring_(scoring),
        station_ready_(std::move(station_ready)) {}
    static constexpr unsigned bank_count = 2, targets_per_bank = 3;
    static constexpr unsigned count = bank_count * targets_per_bank;
    static constexpr std::int32_t raised = 0, lowered = 1;
    static constexpr std::int32_t bank_a_reset = rule_time::half_second;
    static constexpr std::int32_t bank_b_reset = rule_time::three_quarter_second;
    static std::int32_t state(const SharedGameState&, const LampState&, unsigned target);
    static void state(SharedGameState&, LampState&, unsigned target, std::int32_t down);
    void initialize(SharedGameState&, LampState&) const;     // 0x404bc9..0x404bec
    void clear_progress(TurnState&, SharedGameState&) const; // 0x406998
    // Inline 0x40ff6a..0x410674 in 0x40e9e0. Caller suppresses this on tilt.
    // Returns newly lowered targets. Production scoring uses the injected service;
    // original progress, lamp queues and reset timing execute without substitutes.
    unsigned step(TurnState&, SharedGameState&, LampState&) const;

private:
    unsigned record_hits(TurnState&, SharedGameState&, LampState&, Player&) const;
    void schedule_completed_banks(TurnState&, SharedGameState&, const LampState&) const;
    void complete_target_set(TurnState&, SharedGameState&, Player&) const;
    void advance_reset_timers(TurnState&, SharedGameState&, LampState&) const;
    ScoringRules* scoring_;
    std::function<void()> station_ready_;
};
} // namespace web::rules
