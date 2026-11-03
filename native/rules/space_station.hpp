#pragma once
#include "single_ball_awards.hpp"
#include "drop_targets.hpp"

namespace web::rules {
class SpaceStationEffects {
public:
    virtual ~SpaceStationEffects() = default;
    virtual unsigned station_animation_active() const = 0;
    virtual void station_stop_animations() = 0;
    virtual void station_finish_animations() {
        // Recovered stop request for controller-only original-code fixtures.
        // The live display also removes queued station presentation on exit.
        if (station_animation_active() == 19)
            station_stop_animations();
    }
};
// Original station capture and multiball awards. Physical release/serving remains
// in BallLifecycle; capture mutates the actual ball synchronously in physics.
class SpaceStationRules {
public:
    SpaceStationRules(
        TurnState& rules,
        SharedGameState& session,
        physics::SimulationState& simulation,
        ScoringRules& scoring,
        SingleBallAwardEffects* awards = nullptr,
        SpaceStationEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        simulation_(simulation),
        scoring_(scoring),
        awards_(awards),
        effects_(effects) {}
    void capture(std::size_t slot); // 0x408d94
    void ready();                   // 0x408d1c
    void centre_jackpot();          // 0x408770; centre caller 0x410991
    void ramp_jackpot();            // 0x4089d4; web-ramp caller 0x407a27
    void triple_jackpot();          // 0x408c64
    void cleanup();                 // 0x40ec24..0x40ec9c
    void upper_hole();              // 0x40fcdb..0x40ff69
    void timers();                  // 0x410aac..0x410ae8
private:
    TurnState& rules_;
    SharedGameState& session_;
    physics::SimulationState& simulation_;
    ScoringRules& scoring_;
    SingleBallAwardEffects* awards_;
    SpaceStationEffects* effects_;
    Player& player();
    void animation(unsigned id, std::uint32_t flags);
    void jackpot_art(unsigned title, unsigned idle);
    void collect(std::uint32_t units);
};
} // namespace web::rules
