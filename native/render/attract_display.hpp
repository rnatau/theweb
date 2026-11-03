#pragma once
#include "dot_matrix.hpp"
#include "../rules/score_records.hpp"

namespace web {
struct AttractState {
    std::int32_t page = 0, timer = 2, duration = 0;
    std::int32_t transition = 0, credit_cycles = 0, skip = 0, score_font = 0;
};
struct AttractValues {
    std::uint32_t held = 0;
    std::uint32_t edges = 0;
    int previous_players = 0, credits = 0;
    bool repeated_simulation = false;
    rules::ScoreValue replay;
    std::array<rules::ScoreValue, 4> scores{};
    rules::ScoreRecords records;
};
// Idle branch of original 0x41cf10. State advances only when a DMD frame draws.
class AttractDisplay {
public:
    AttractState state;
    DotMatrixFrame render(const DotMatrixRenderer&, const AttractValues&, const DotMatrixFrame& backdrop);
    static std::string score_code(std::uint32_t kind, const std::array<std::uint8_t, 4>& initials, std::uint32_t value, std::uint32_t parameter); // 0x41bedc.
private:
    void ranking(const DotMatrixRenderer&, DotMatrixFrame&, const rules::ScoreRecord&, int y);
};
} // namespace web
