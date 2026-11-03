#pragma once
#include "score_records.hpp"
#include "../physics/integer.hpp"

namespace web::rules {
// Original input-repeat tail, 0x418482..0x4184d1. Fresh edges remain distinct
// from the repeated held actions consumed by the initials editor.
struct ActionRepeater {
    std::int32_t counter = 0;
    std::uint32_t step(std::uint32_t held, std::uint32_t& edges) {
        constexpr int initial_delay = 40, repeat_restart = 35;
        edges &= held;
        auto repeated = edges;
        if (!held)
            counter = 0;
        counter = physics::integer::add(counter, 1);
        if (counter > initial_delay) {
            counter = repeat_restart;
            repeated = held;
        }
        return repeated;
    }
};
enum class RecordPage : unsigned {
    initials = 66,
    combo = 71,
    loop = 72,
    score = 73
};
class RecordEffects {
public:
    virtual ~RecordEffects() = default;
    virtual void record_page(RecordPage, unsigned ticks, int priority) = 0;
    virtual bool record_page_active(RecordPage) const {
        return false;
    }
};
inline void initialize_saved_initials(PlayerRoster& players) {
    for (auto& player : players)
        player.records.saved_initials.fill('A');
}
// Original initials entry. The caller supplies original edge/repeat actions and
// the shared replay-feedback delay; confirmation waits for queued replays.
class RecordEntryRules {
public:
    RecordEntryRules(TurnState& rules, SharedGameState& session, physics::SimulationState& simulation, ReplayState& replay, std::int32_t& replay_delay, RecordEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        simulation_(simulation),
        replay_(replay),
        replay_delay_(replay_delay),
        effects_(effects) {}
    bool step(std::uint32_t& edges, std::uint32_t& repeats); // 0x410c58.
    void sound_sink(SoundSink* sink) {
        sounds_ = sink;
    }

private:
    TurnState& rules_;
    SharedGameState& session_;
    physics::SimulationState& simulation_;
    ReplayState& replay_;
    std::int32_t& replay_delay_;
    RecordEffects* effects_;
    SoundSink* sounds_ = nullptr;
    void sound(SoundRequest request) {
        if (sounds_)
            sounds_->sound(request);
    }
    std::uint8_t character(unsigned index) const;
    void character(unsigned index, std::uint8_t value);
    std::uint32_t saved_character(unsigned player, unsigned index) const;
    void saved_character(unsigned player, unsigned index, std::uint32_t value);
};
} // namespace web::rules
