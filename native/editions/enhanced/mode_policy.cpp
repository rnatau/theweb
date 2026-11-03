#include "mode_policy.hpp"

namespace web::enhanced {
bool collection_allowed(const rules::TurnState& state, const rules::SharedGameState& session) {
    namespace F = web::rules::turn_field;

    if (state.tilted || state.mission_time || state.live_balls > 1 || state.video_active || state.video_lock_pending || state.lock_choice_timer || state.explosion_wait ||
        state.flipper_timing_window || state.showdown_intro || state.showdown_phase > 1 || state.showdown_stage >= 4)
        return false;
    const auto current = state.current_player;
    if (current < 0 || current >= 4)
        return false;
    const auto& player = session.players().at(unsigned(current));
    for (auto mode : {player.launcher.state, player.combos.state, player.quick_shot.state, player.lock.state, player.station.state, player.awards.explosion_ready})
        if (mode == rules::mode_state::active)
            return false;
    return true;
}
} // namespace web::enhanced
