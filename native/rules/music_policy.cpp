#include "music_policy.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;

namespace S = web::rules::session_field;
namespace {
constexpr int fade_duration = 3 * rule_time::second, minimum_y = 0xa00000, maximum_speed = 0x1000;
constexpr unsigned normal_track_id = 10, docked_track_id = 13;
} // namespace
unsigned MusicPolicy::default_track(const Player& player) {
    return player.station.state == mode_state::ready && player.station.captures == 2 ? docked_track_id : normal_track_id;
}
bool MusicPolicy::ready(std::int32_t policy) {
    if (!policy) {
        rules_.music_fade_timer = std::max(0, physics::integer::sub(rules_.music_fade_timer, rules_.elapsed));
        if (!rules_.music_fade_timer)
            return true;
    }
    for (const auto& ball : simulation_.balls) {
        if (!ball.active())
            continue;
        if (ball.motion_type() == physics::ball_type::pending_drain)
            return false;
        if (ball.motion_type() != physics::ball_type::free)
            continue;
        if (!policy)
            return false;
        if (sounds_ && (sounds_->active(-1, -1, 1) || sounds_->active(-1, -1, 2)))
            return false;
        if (!(std::uint32_t(policy) & 1))
            return false;
        if (ball.position().y < minimum_y)
            return false;
        const auto velocity = ball.velocity();
        if (velocity.x < -maximum_speed || velocity.x > maximum_speed || velocity.y < -maximum_speed || velocity.y > maximum_speed)
            return false;
    }
    return true;
}
void MusicPolicy::step(std::int32_t policy) {
    if (rules_.mission_time || rules_.single_ball_award_mode == 2 || rules_.video_active || rules_.showdown_intro || rules_.drain_override)
        return;
    const auto& player = session_.players().at(std::uint32_t(rules_.current_player));
    const auto desired = default_track(player);
    if (session_.music_track == std::int32_t(desired))
        return;
    if (!policy && !rules_.music_fade_timer) {
        rules_.music_fade_timer = fade_duration;
        if (effects_)
            effects_->fade_music(desired);
        rules_.music_requested_track = std::int32_t(desired);
    }
    if (!ready(policy))
        return;
    rules_.music_fade_timer = 0;
    if (sounds_)
        sounds_->stop();
    session_.request_music_track(std::int32_t(desired));
    if (effects_)
        effects_->start_music(desired);
}
} // namespace web::rules
