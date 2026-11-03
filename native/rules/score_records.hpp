#pragma once
#include "scoring.hpp"

namespace web::rules {
struct ScoreRecord {
    std::array<std::uint8_t, 4> initials{};
    ScoreValue score;
};
struct ChampionRecord {
    std::array<std::uint8_t, 4> initials{};
    std::uint32_t count = 0, reserved = 0;
};
// Named interpretation of the already verified original game.dat payload.
// The session ranking is intentionally separate from the persistent records.
class ScoreRecords {
public:
    static constexpr unsigned ranks = 5;
    ScoreRecords();                         // Original missing-save defaults at 0x413770.
    void load(const Scores&, ReplayState&); // Reader mapping, then original session defaults.
    Scores save(const ReplayState&) const;  // Original writer mapping at 0x41ba34.
    std::array<ScoreRecord, ranks>& ordinary() {
        return ordinary_;
    }
    const std::array<ScoreRecord, ranks>& ordinary() const {
        return ordinary_;
    }
    std::array<ScoreRecord, ranks>& buyin() {
        return buyin_;
    }
    const std::array<ScoreRecord, ranks>& buyin() const {
        return buyin_;
    }
    std::array<ScoreRecord, ranks>& session() {
        return session_;
    }
    const std::array<ScoreRecord, ranks>& session() const {
        return session_;
    }
    ChampionRecord& loops() {
        return loops_;
    }
    const ChampionRecord& loops() const {
        return loops_;
    }
    ChampionRecord& combos() {
        return combos_;
    }
    const ChampionRecord& combos() const {
        return combos_;
    }

private:
    std::array<ScoreRecord, ranks> ordinary_, buyin_, session_;
    ChampionRecord loops_, combos_;
    void initialize_session();
};
} // namespace web::rules
