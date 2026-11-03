#include "serve.hpp"
#include "music_policy.hpp"
#include "../physics/integer.hpp"

namespace web::rules {
namespace F = web::rules::turn_field;

bool ServeRules::step() {
    SessionRules session_rules;
    if (rules_.phase() == GamePhase::begin_ball) {
        session_rules.clear_lamp_animation(rules_, session_);
        state_.redraw_lamps = 1;
        rules_.live_balls = 1;
        rules_.phase(GamePhase::serve_ball);
        return true;
    }
    if (rules_.phase() != GamePhase::serve_ball)
        return false;
    session_rules.setup_ball(simulation_, rules_, session_);
    rules_.launch_enabled = 1;
    rules_.music_fade_timer = 0;
    session_effects_.on_effect(
        web::rules::SessionEffect{
            web::SoundStopRequest{-1, -1, -1}
    },
        simulation_,
        rules_,
        session_);
    const auto& p = session_.players().at(std::uint32_t(rules_.current_player));
    session_.request_music_track(std::int32_t(MusicPolicy::default_track(p)));
    if (rules_.announce_pending)
        session_rules.announce(simulation_, rules_, session_, session_effects_);
    if (rules_.extra_ball_turn) {
        const auto choice = (rules_.frame_phase() >> 4) & 3;
        constexpr int samples[] = {2, 2, 91, 82};
        constexpr int pans[] = {64, 64, -64, 0};
        session_effects_.on_effect(
            web::rules::SessionEffect{
                web::SoundRequest{samples[choice], pans[choice], 0, 1}
        },
            simulation_,
            rules_,
            session_);
        if (awards_)
            awards_->award_animation(70, 1);
    }
    if (p.ball.number == 3) {
        const auto duration = rules_.player_count == 1 || rules_.extra_ball_turn ? 24u : 36u;
        if (effects_)
            effects_->serve_page(ServePage::last_ball, duration, 0);
    } else if (rules_.player_count > 1 && effects_)
        effects_->serve_page(ServePage::player, 24, 0);
    rules_.extra_ball_turn = 0;
    rules_.phase(GamePhase::playing);
    rules_.serve_requests = physics::integer::add(rules_.serve_requests, 1);
    return true;
}
} // namespace web::rules
