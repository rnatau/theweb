#pragma once
#include "record_entry.hpp"
#include <optional>

namespace web::rules {
struct RecordFlowState {
    bool entering_pre_buyin = false;
};
// Original phase-five records controller and insertion helpers. Persistent
// records, transient rankings and shared rule bytes retain separate owners.
class RecordFlowRules {
public:
    RecordFlowRules(
        TurnState& rules,
        SharedGameState& session,
        physics::SimulationState& simulation,
        ScoreRecords& records,
        ReplayState& replay,
        std::int32_t& replay_delay,
        RecordFlowState& state,
        RecordEffects* effects = nullptr,
        bool improved_records = false) :
        rules_(rules),
        session_(session),
        simulation_(simulation),
        records_(records),
        replay_(replay),
        replay_delay_(replay_delay),
        state_(state),
        effects_(effects),
        entry_(rules, session, simulation, replay, replay_delay, effects),
        improved_records_(improved_records) {}
    void step(std::uint32_t& edges, std::uint32_t& repeats); // 0x4112e0.
    void insert_ordinary();                                  // 0x410ef8.
    void insert_buyin();                                     // 0x411000.
    void insert_session();                                   // 0x4110e8.
    void finish_session_entry();                             // 0x4111a8.
    void sound_sink(SoundSink* sink) {
        sounds_ = sink;
        entry_.sound_sink(sink);
    }

private:
    TurnState& rules_;
    SharedGameState& session_;
    physics::SimulationState& simulation_;
    ScoreRecords& records_;
    ReplayState& replay_;
    std::int32_t& replay_delay_;
    RecordFlowState& state_;
    RecordEffects* effects_;
    RecordEntryRules entry_;
    SoundSink* sounds_ = nullptr;
    bool improved_records_;
    bool complete_initials(std::uint32_t& edges, std::uint32_t& repeats);
    void announce_record(int sample);
    Player& player(unsigned index);
    ScoreValue player_score(unsigned index);
    ScoreValue selected_score() const;
    static bool at_least(ScoreValue left, ScoreValue right);
    int find_rank(const std::array<ScoreRecord, 5>&, ScoreValue, unsigned first);
    void insert(std::array<ScoreRecord, 5>&, unsigned rank);
    void begin_score_entry(int stage, unsigned display_rank);
    void grant_replays(unsigned count);
    void copy_initials(std::array<std::uint8_t, 4>&);
    enum class ChampionKind {
        loops,
        combos
    };
    void find_champion(ChampionKind, ChampionRecord&, RecordPage);
    bool page_wait(RecordPage, std::uint32_t edges);
};
} // namespace web::rules
