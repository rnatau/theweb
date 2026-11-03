#pragma once
#include "single_ball_awards.hpp"

namespace web::rules {
enum class ServePage : unsigned {
    player = 67,
    last_ball = 69
};
struct ServeState {
    std::int32_t redraw_lamps = 0;
}; // Original 0x42c4e4.
class ServeEffects {
public:
    virtual ~ServeEffects() = default;
    virtual void serve_page(ServePage, unsigned ticks, int priority) = 0;
};
// Original phases one/two: clear the lamp grid, reset this player's ball,
// announce it, request the appropriate page and queue the physical serve.
class ServeRules {
public:
    ServeRules(
        TurnState& rules,
        SharedGameState& session,
        physics::SimulationState& simulation,
        ServeState& state,
        SessionEffectSink& session_effects,
        SingleBallAwardEffects* awards = nullptr,
        ServeEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        simulation_(simulation),
        state_(state),
        session_effects_(session_effects),
        awards_(awards),
        effects_(effects) {}
    bool step(); // Original 0x411d74..0x411ec1.
private:
    TurnState& rules_;
    SharedGameState& session_;
    physics::SimulationState& simulation_;
    ServeState& state_;
    SessionEffectSink& session_effects_;
    SingleBallAwardEffects* awards_;
    ServeEffects* effects_;
};
} // namespace web::rules
