#pragma once
#include "missions.hpp"
#include <functional>

namespace web::rules {
enum class CentreRequest {
    collect_ramp_multiball,
    collect_lock_multiball,
    start_showdown,
    start_single_ball_award,
    unlit_hole
};
// Original post-target centre shot / hole / award wait dispatch,
// 0x4107c6..0x410a29. Remaining multiball/Showdown calls are synchronous requests.
class CentreShotRules {
public:
    CentreShotRules(TurnState& rules, SharedGameState& session, ScoringRules& scoring, ExtraBallRules& extra_balls, MissionRules& missions, std::function<void(CentreRequest)> remaining = {}) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        extra_balls_(extra_balls),
        missions_(missions),
        remaining_(std::move(remaining)) {}
    void step(bool collect_mode = true);

private:
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    ExtraBallRules& extra_balls_;
    MissionRules& missions_;
    std::function<void(CentreRequest)> remaining_;
    void collect_award();
    void dispatch(bool collect_mode);
    void request(CentreRequest kind);
};
} // namespace web::rules
