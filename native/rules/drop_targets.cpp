#include "drop_targets.hpp"
#include "scoring.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr unsigned hit_award_hundreds = 10;
constexpr SoundRequest target_hit_sound{.id = 181, .pan = 64, .attenuation = 64, .group = 17};
constexpr SoundRequest bank_reset_sound{.id = 152, .pan = 32, .attenuation = 0, .group = 11};
constexpr SoundRequest station_ready_voice{.id = 122, .pan = -64, .attenuation = 0, .group = 1};
constexpr SoundRequest super_jackpot_ready_voice{.id = 49, .pan = 64, .attenuation = 0, .group = 1};
constexpr int hit_flash = rule_time::quarter_second, completion_flash = rule_time::second;
unsigned target_insert(unsigned target) {
    return unsigned(TableLamp::drop_target_first) + target;
}
} // namespace

std::int32_t DropTargetRules::state(const SharedGameState&, const LampState& lamps, unsigned target) {
    if (target >= count)
        throw std::out_of_range("Invalid drop target");
    return lamps.drop_targets.at(target);
}
void DropTargetRules::state(SharedGameState&, LampState& lamps, unsigned target, std::int32_t down) {
    if (target >= count)
        throw std::out_of_range("Invalid drop target");
    lamps.drop_targets.at(target) = down;
}
void DropTargetRules::initialize(SharedGameState& session, LampState& lamps) const {
    for (unsigned target = 0; target < count; ++target)
        state(session, lamps, target, lowered);
}
void DropTargetRules::clear_progress(TurnState& rules, SharedGameState& session) const {
    auto& player = session.players().at(std::uint32_t(rules.current_player));
    for (unsigned target = 0; target < count; ++target)
        player.targets.at(target) = 0;
    rules.target_bank_b_timer = bank_b_reset;
    rules.target_bank_a_timer = bank_a_reset;
}
unsigned DropTargetRules::record_hits(TurnState& rules, SharedGameState& session, LampState& lamps, Player& player) const {
    LampRules lights;
    unsigned hits = 0;
    for (unsigned target = 0; target < count; ++target) {
        const auto contact = std::size_t(TableSwitch::drop_target_first) + target;
        if (state(session, lamps, target) != raised || !rules.switch_edge(contact))
            continue;
        if (scoring_)
            scoring_->hundreds(hit_award_hundreds);
        if (scoring_)
            scoring_->sound(target_hit_sound);
        lights.queue(rules, session, target_insert(target), hit_flash);
        state(session, lamps, target, lowered);
        player.targets.at(target) = 1;
        ++hits;
    }
    return hits;
}
void DropTargetRules::schedule_completed_banks(TurnState& rules, SharedGameState& session, const LampState& lamps) const {
    constexpr std::array timers = {F::target_bank_a_timer, F::target_bank_b_timer};
    constexpr int durations[] = {bank_a_reset, bank_b_reset};
    LampRules lights;
    for (unsigned bank = 0; bank < bank_count; ++bank) {
        const auto first = bank * targets_per_bank;
        if ((rules.*timers[bank]) == 0 && state(session, lamps, first) != raised && state(session, lamps, first + 1) != raised && state(session, lamps, first + 2) != raised) {
            (rules.*timers[bank]) = durations[bank];
            for (unsigned target = first; target < first + targets_per_bank; ++target)
                lights.queue(rules, session, target_insert(target), completion_flash);
        }
    }
}
void DropTargetRules::complete_target_set(TurnState& rules, SharedGameState& session, Player& player) const {
    bool completed = true;
    for (unsigned target = 0; target < count; ++target)
        completed &= player.targets.at(target) != 0;
    if (!completed || (session.mission_kind == 0 && rules.mission_time != 0))
        return;

    LampRules lights;
    const auto super_stage = rules.station_super_stage();
    if (super_stage == StationSuperStage::inactive) {
        // Completion clears player credit and schedules physical resets before
        // lighting the magnetic hold / station dock; retain this original order.
        clear_progress(rules, session);
        if (rules.showdown_phase != mode_state::active) {
            lights.queue(rules, session, TableLamp::magnetic_ready, completion_flash);
            player.magnet.ready = 1;
            if (player.station.state == mode_state::inactive) {
                if (station_ready_)
                    station_ready_();
                else {
                    if (scoring_)
                        scoring_->sound(station_ready_voice);
                    player.station.state = mode_state::ready;
                    lights.queue(rules, session, TableLamp::station_dock, completion_flash);
                }
            }
        }
    } else {
        if (super_stage == StationSuperStage::targets_required) {
            if (scoring_)
                scoring_->sound(super_jackpot_ready_voice);
            session.station_super_holdoff = rule_time::second;
            rules.station_super_stage(StationSuperStage::magnetic_hole_required);
            lights.queue(rules, session, TableLamp::station_magnetic_hole, completion_flash);
        }
        rules.station_super_lit = 1;
    }
}
void DropTargetRules::advance_reset_timers(TurnState& rules, SharedGameState& session, LampState& lamps) const {
    // Original resets bank B before A; zero timers do not raise a bank. A hit
    // and an expiring pre-existing reset can occur on the same rules update.
    for (auto [timer, first] : {
             std::pair{F::target_bank_b_timer, targets_per_bank},
             std::pair{F::target_bank_a_timer, 0u              }
    }) {
        if (const auto before = (rules.*timer); before != 0) {
            const auto after = std::max(0, sub(before, rules.elapsed));
            (rules.*timer) = after;
            if (after == 0) {
                if (scoring_)
                    scoring_->sound(bank_reset_sound);
                for (unsigned target = first; target < first + targets_per_bank; ++target)
                    state(session, lamps, target, raised);
            }
        }
    }
}
unsigned DropTargetRules::step(TurnState& rules, SharedGameState& session, LampState& lamps) const {
    auto& player = session.players().at(std::uint32_t(rules.current_player));
    const auto hits = record_hits(rules, session, lamps, player);
    // Fast Frenzy holds completed banks down, but previously scheduled reset
    // timers still run. Hits must be processed before either completion branch.
    if (player.lock.state != mode_state::active) {
        schedule_completed_banks(rules, session, lamps);
        complete_target_set(rules, session, player);
    }
    advance_reset_timers(rules, session, lamps);
    return hits;
}
} // namespace web::rules
