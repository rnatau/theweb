#include "start_rules.hpp"

namespace web::enhanced {
bool HoldStart::operator()(std::uint32_t actions, const rules::TurnState& rules, int refresh_rate) {
    namespace F = web::rules::turn_field;
    if (!(actions & input_action::start) || !rules.player_count || rules.start_latch == -1 || rules.phase() == rules::GamePhase::records) {
        held_ticks_ = 0;
        return false;
    }
    return ++held_ticks_ == unsigned(refresh_rate);
}
rules::SessionRules::StartResult start(physics::SimulationState& simulation, rules::TurnState& rules, rules::SharedGameState& session, rules::SessionEffectSink& effects) {
    namespace F = web::rules::turn_field;
    using Result = rules::SessionRules::StartResult;
    constexpr int maximum_players = 4;
    constexpr int rejected_start_sound = 166; // X0020001.pcm (sample 156).
    // Startup and initials retain their input ownership; S is not an error cue
    // while the original startup gate or records controller owns it.
    if (rules.start_latch == -1 || rules.phase() == rules::GamePhase::records)
        return Result::ignored;
    const auto count = rules.player_count;
    if (count) {
        const auto current = rules.current_player;
        if (count < 0 || count >= maximum_players || current < 0 || current >= count || session.players().at(unsigned(current)).ball.number != 1) {
            effects.on_effect(
                web::rules::SessionEffect{
                    web::SoundRequest{rejected_start_sound, 0, 0, -1}
            },
                simulation,
                rules,
                session);
            return Result::rejected;
        }
    }
    // Only the add-player branch can be reached. The recovered initializer,
    // announcements and first-game serving handoff remain shared.
    return rules::SessionRules{}.start(simulation, rules, session, effects);
}
} // namespace web::enhanced
