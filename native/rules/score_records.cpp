#include "score_records.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace {
using Initials = std::array<std::uint8_t, 4>;
constexpr Initials name(char a, char b, char c) {
    return {std::uint8_t(a), std::uint8_t(b), std::uint8_t(c), 0};
}
constexpr std::array<Initials, 5> ordinary_names = {name('A', 'D', 'E'), name('A', 'D', 'E'), name('T', 'K', '.'), name('C', 'M', 'P'), name('A', 'D', 'P')};
constexpr std::array<Initials, 5> buyin_names = {name('X', 'X', 'X'), name('E', 'G', 'G'), name('B', 'A', 'Z'), name('W', 'J', 'B'), name('R', 'I', 'K')};
constexpr std::array<ScoreValue, 5> default_scores = {
    {{0, 1}, {80000000, 0}, {60000000, 0}, {40000000, 0}, {20000000, 0}}
};
void put(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes[offset + i] = std::uint8_t(value >> (i * 8));
}
ScoreRecord read_score(std::span<const std::uint8_t> data, std::size_t offset) {
    ScoreRecord result;
    std::copy_n(data.begin() + offset, 4, result.initials.begin());
    result.score = {u32(data, offset + 8), u32(data, offset + 4)};
    return result;
}
ChampionRecord read_champion(std::span<const std::uint8_t> data, std::size_t offset) {
    ChampionRecord result;
    std::copy_n(data.begin() + offset, 4, result.initials.begin());
    result.count = u32(data, offset + 4);
    result.reserved = u32(data, offset + 8);
    return result;
}
void write_score(std::span<std::uint8_t> data, std::size_t offset, const ScoreRecord& value) {
    std::copy(value.initials.begin(), value.initials.end(), data.begin() + offset);
    put(data, offset + 4, value.score.rollovers);
    put(data, offset + 8, value.score.units);
}
void write_champion(std::span<std::uint8_t> data, std::size_t offset, const ChampionRecord& value) {
    std::copy(value.initials.begin(), value.initials.end(), data.begin() + offset);
    put(data, offset + 4, value.count);
    put(data, offset + 8, value.reserved);
}
} // namespace
ScoreRecords::ScoreRecords() {
    for (unsigned i = 0; i < ranks; ++i) {
        ordinary_[i] = {ordinary_names[i], default_scores[i]};
        buyin_[i] = {buyin_names[i], default_scores[i]};
    }
    loops_ = {name('C', 'M', 'P'), 3, 0};
    combos_ = {name('T', 'K', '.'), 3, 0};
    initialize_session();
}
void ScoreRecords::initialize_session() {
    for (unsigned i = 0; i < ranks; ++i)
        session_[i] = {ordinary_names[i], default_scores[i]};
    session_[0].initials = name('X', 'X', 'X');
}
void ScoreRecords::load(const Scores& scores, ReplayState& replay) {
    const auto data = std::span(scores.payload);
    replay.threshold_units = u32(data, 0);
    replay.threshold_rollovers = physics::integer::bits(u32(data, 4));
    replay.credits = physics::integer::bits(u32(data, 8));
    for (unsigned i = 0; i < ranks; ++i) {
        ordinary_[i] = read_score(data, 12 + i * 12);
        buyin_[i] = read_score(data, 72 + i * 12);
    }
    loops_ = read_champion(data, 132);
    combos_ = read_champion(data, 144);
    initialize_session();
}
Scores ScoreRecords::save(const ReplayState& replay) const {
    Scores result;
    auto data = std::span(result.payload);
    put(data, 0, replay.threshold_units);
    put(data, 4, std::uint32_t(replay.threshold_rollovers));
    put(data, 8, std::uint32_t(replay.credits));
    for (unsigned i = 0; i < ranks; ++i) {
        write_score(data, 12 + i * 12, ordinary_[i]);
        write_score(data, 72 + i * 12, buyin_[i]);
    }
    write_champion(data, 132, loops_);
    write_champion(data, 144, combos_);
    return result;
}
} // namespace web::rules
