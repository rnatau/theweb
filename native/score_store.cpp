#include "score_store.hpp"
#include "rules/score_records.hpp"
#include <algorithm>

namespace web {
void validate_playable_scores(const Scores& scores) {
    rules::ScoreRecords records;
    rules::ReplayState replay;
    records.load(scores, replay);
    const auto score = [](rules::ScoreValue value) {
        if (value.units >= rules::ScoreValue::radix || value.rollovers > 999)
            throw std::runtime_error("Score file contains an unsupported score value");
    };
    const auto initials = [](const auto& name) {
        for (auto c : name) {
            if (!c)
                break;
            if (c < 32 || c > 93)
                throw std::runtime_error("Score file contains unsupported initials");
        }
    };
    if (replay.credits < 0 || replay.credits > 9 || replay.threshold_rollovers < 0)
        throw std::runtime_error("Score file contains unsupported replay values");
    score({replay.threshold_units, std::uint32_t(replay.threshold_rollovers)});
    for (const auto* table : {&records.ordinary(), &records.buyin()})
        for (const auto& record : *table) {
            score(record.score);
            initials(record.initials);
        }
    for (const auto* champion : {&records.loops(), &records.combos()}) {
        initials(champion->initials);
        if (champion->count > 0x7fffffffu)
            throw std::runtime_error("Score file contains an unsupported champion count");
    }
}

ScoreStore::ScoreStore(std::filesystem::path directory, std::unique_ptr<SaveStorage> storage) :
    file_(std::move(directory), "game.dat", Scores::record_bytes, [](std::span<const std::uint8_t> bytes) { validate_playable_scores(parse_scores(bytes)); }, std::move(storage)) {}
std::optional<Scores> ScoreStore::load() {
    if (const auto bytes = file_.load())
        return parse_scores(*bytes);
    return {};
}
bool ScoreStore::save(const Scores& scores) {
    return file_.save(serialize_scores(scores));
}
} // namespace web
