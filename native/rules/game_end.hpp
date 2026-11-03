#pragma once
#include "record_flow.hpp"

namespace web::rules {
class GameEndEffects {
public:
    virtual ~GameEndEffects() = default;
    virtual void match_page(unsigned ticks, int priority) = 0;
    virtual void end_clear_page() = 0;
    virtual void end_stop_animations() = 0;
};
struct GameEndState {
    RecordFlowState records;
    std::int32_t previous_players = 0; // 0x42c504.
};
// Original records, match and replay-threshold cleanup, phases five to seven.
class GameEndRules {
public:
    GameEndRules(
        TurnState& rules,
        SharedGameState& session,
        physics::SimulationState& simulation,
        ScoreRecords& records,
        ReplayState& replay,
        std::int32_t& replay_delay,
        GameEndState& state,
        RecordEffects* record_effects = nullptr,
        GameEndEffects* effects = nullptr,
        bool improved_records = false) :
        rules_(rules),
        session_(session),
        simulation_(simulation),
        replay_(replay),
        state_(state),
        replay_delay_(replay_delay),
        effects_(effects),
        records_(rules, session, simulation, records, replay, replay_delay, state.records, record_effects, improved_records) {}
    void feedback(); // Shared replay queue at 0x411c89..0x411d01.
    void sound_sink(SoundSink* sink) {
        sounds_ = sink;
        records_.sound_sink(sink);
    }
    void step(std::uint32_t& edges, std::uint32_t& repeats);
    void match();  // 0x4124ba..0x4126ce.
    void finish(); // 0x405340 and phase seven.
private:
    TurnState& rules_;
    SharedGameState& session_;
    physics::SimulationState& simulation_;
    ReplayState& replay_;
    GameEndState& state_;
    std::int32_t& replay_delay_;
    GameEndEffects* effects_;
    RecordFlowRules records_;
    SoundSink* sounds_ = nullptr;
    void random_digit();
};
} // namespace web::rules
