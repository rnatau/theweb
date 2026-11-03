#include "centre_shot.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;

using namespace physics::integer;
namespace {
constexpr int award_duration = (5 * rule_time::half_second), contact_flash = rule_time::quarter_second;
constexpr int ready = 1, active = 2;
constexpr unsigned centre_insert = 68;
} // namespace
void CentreShotRules::request(CentreRequest kind) {
    if (remaining_)
        remaining_(kind);
}
void CentreShotRules::collect_award() {
    auto& p = session_.players().at(std::uint32_t(rules_.current_player));
    if (!rules_.extra_ball_diverter && !p.awards.extra_balls_lit)
        return;
    rules_.centre_award_consumed = 1;
    if (rules_.extra_ball_diverter)
        extra_balls_.collect_diverter();
    else
        extra_balls_.collect_lit();
    // Both original arms use 0x14000. The diverter arm preserves ECX across
    // 0x408490; decompiler 'extraout_ECX' must not be mistaken for zero.
    rules_.centre_award_wait = award_duration;
    if (!rules_.drain_override && !rules_.mission_time && rules_.single_ball_award_mode != active)
        rules_.returns_paused = 1;
}
void CentreShotRules::dispatch(bool collect_mode) {
    auto& p = session_.players().at(std::uint32_t(rules_.current_player));
    if (rules_.drain_override) {
        if (p.station.state == active)
            request(CentreRequest::collect_ramp_multiball);
        if (p.lock.state == active && !rules_.combined_lock)
            request(CentreRequest::collect_lock_multiball);
    } else if (!collect_mode)
        return;
    else if (rules_.showdown_phase == ready)
        request(CentreRequest::start_showdown);
    else if (rules_.single_ball_award_mode == ready)
        request(CentreRequest::start_single_ball_award);
    else if (!rules_.mission_time && !rules_.single_ball_award_mode) {
        if (rules_.drain_extension) {
            rules_.mission_started_this_frame = 1;
            missions_.start();
        } else if (!rules_.centre_award_consumed)
            request(CentreRequest::unlit_hole);
    }
}
void CentreShotRules::step(bool collect_mode) {
    if (rules_.switch_edge(TableSwitch::centre_shot)) {
        LampRules{}.queue(rules_, session_, centre_insert, contact_flash);
        missions_.advance_selection();
        if (session_.mission_centre_lit) {
            const bool alternate = ((rules_.frame_phase() >> 4) & 1) != 0;
            scoring_.sound({alternate ? 104 : 51, alternate ? -64 : 64, 0, 1});
        }
        scoring_.sound({173, 0, 64, 18});
    }
    rules_.mission_started_this_frame = 0;
    rules_.centre_award_consumed = 0;
    if (!rules_.switch_edge(TableSwitch::centre_sinkhole) && !rules_.centre_award_wait)
        return;
    if (!rules_.centre_award_wait)
        collect_award();
    else {
        const auto remaining = std::max(0, sub(rules_.centre_award_wait, rules_.elapsed));
        rules_.centre_award_wait = remaining;
        if (!remaining)
            rules_.returns_paused = 0;
    }
    if (!rules_.centre_award_wait)
        dispatch(collect_mode);
}
} // namespace web::rules
