#pragma once
#include "session_rules.hpp"
#include "../sound.hpp"

namespace web::rules {
class MusicEffects {
public:
    virtual ~MusicEffects() = default;
    virtual void fade_music(unsigned track) = 0;
    virtual void start_music(unsigned track) = 0;
};
// Original transition policy, independent of CD/file playback. Music metadata,
// sound queries and effects remain explicit; callers own the selected policy.
class MusicPolicy {
public:
    MusicPolicy(TurnState& rules, SharedGameState& session, physics::SimulationState& simulation, SoundSink* sounds = nullptr, MusicEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        simulation_(simulation),
        sounds_(sounds),
        effects_(effects) {}
    bool ready(std::int32_t restart_policy); // 0x40e1ac.
    void step(std::int32_t restart_policy);  // 0x40e2d4.
    static unsigned default_track(const Player&);

private:
    TurnState& rules_;
    SharedGameState& session_;
    physics::SimulationState& simulation_;
    SoundSink* sounds_;
    MusicEffects* effects_;
};
} // namespace web::rules
