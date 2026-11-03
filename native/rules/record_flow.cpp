#include "record_flow.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr auto rank = F::record_rank;
constexpr int record_music_track = 12;
} // namespace
Player& RecordFlowRules::player(unsigned i) {
    return session_.players().at(i);
}
ScoreValue RecordFlowRules::player_score(unsigned i) {
    const auto& p = player(i);
    return {std::uint32_t(p.score.units), std::uint32_t(p.score.rollovers)};
}
ScoreValue RecordFlowRules::selected_score() const {
    return {std::uint32_t(rules_.record_best_low), std::uint32_t(rules_.record_best_high)};
}
bool RecordFlowRules::at_least(ScoreValue left, ScoreValue right) {
    const auto high_left = bits(left.rollovers), high_right = bits(right.rollovers);
    return high_left > high_right || (high_left == high_right && bits(left.units) >= bits(right.units));
}
int RecordFlowRules::find_rank(const std::array<ScoreRecord, 5>& table, ScoreValue score, unsigned first) {
    int result = -1;
    for (unsigned i = first; i < table.size(); ++i)
        if (at_least(score, table[i].score)) {
            result = int(i);
            break;
        }
    rules_.record_rank = result;
    return result;
}
void RecordFlowRules::insert(std::array<ScoreRecord, 5>& table, unsigned row) {
    if (row >= table.size())
        throw std::out_of_range("Invalid score insertion rank");
    for (unsigned i = unsigned(table.size() - 1); i > row; --i)
        table[i] = table[i - 1];
    table[row].score = selected_score();
}
void RecordFlowRules::grant_replays(unsigned count) {
    while (count--) {
        replay_.awards = add(replay_.awards, 1);
        replay_.credits = std::min(9, add(replay_.credits, 1));
        replay_delay_ = rule_time::half_second;
    }
}
void RecordFlowRules::announce_record(int sample) {
    if (sounds_)
        sounds_->stop();
    // Original 0x404784 -> 0x415834 keeps this metadata even when optional
    // music playback is unavailable. End-game cleanup tests the selected track.
    session_.request_music_track(record_music_track);
    if (sounds_)
        sounds_->sound({sample, 64, 0, 1});
}
void RecordFlowRules::begin_score_entry(int next, unsigned display) {
    rules_.bonus_display_gate = next;
    rules_.record_display_rank = int(display);
    if (effects_)
        effects_->record_page(RecordPage::score, 48, 1);
    session_.initials_initialized = 0;
    rules_.music_fade_timer = 0;
    announce_record(54);
}
void RecordFlowRules::insert_ordinary() {
    const auto row = unsigned(rules_.record_rank);
    insert(records_.ordinary(), row);
    begin_score_entry(6, row);
    grant_replays(row == 0 ? 3 : row == 1 ? 2 : 1);
}
void RecordFlowRules::insert_buyin() {
    const auto row = unsigned(rules_.record_rank);
    insert(records_.buyin(), row);
    begin_score_entry(7, row + 4);
    grant_replays(row == 1 ? 2 : 1);
}
void RecordFlowRules::insert_session() {
    const auto row = unsigned(rules_.record_rank);
    insert(records_.session(), row);
    begin_score_entry(8, row + 8);
}
void RecordFlowRules::copy_initials(std::array<std::uint8_t, 4>& target) {
    const auto value = std::uint32_t(session_.initials_letters);
    for (unsigned i = 0; i < 3; ++i)
        target[i] = std::uint8_t(value >> (i * 8));
}
void RecordFlowRules::finish_session_entry() {
    const auto row = find_rank(records_.session(), player_score(unsigned(rules_.record_best_player)), 1);
    if (row < 0)
        return;
    insert(records_.session(), unsigned(row));
    copy_initials(records_.session()[unsigned(row)].initials);
}
void RecordFlowRules::find_champion(ChampionKind kind, ChampionRecord& champion, RecordPage page) {
    rules_.record_maximum = 0;
    rules_.record_entry_player = 0;
    rules_.record_scan_index = 0;
    const auto count = rules_.player_count;
    if (count > 4)
        throw std::out_of_range("Invalid record player count");
    for (int i = 0; i < count; ++i) {
        const auto value = (kind == ChampionKind::combos ? player(unsigned(i)).combos.best : player(unsigned(i)).loops.best);
        if (value > rules_.record_maximum) {
            rules_.record_maximum = value;
            rules_.record_entry_player = i;
        }
        rules_.record_scan_index = i + 1;
    }
    if (rules_.record_maximum > bits(champion.count)) {
        champion.count = std::uint32_t(rules_.record_maximum);
        if (effects_)
            effects_->record_page(page, 48, 1);
        rules_.music_fade_timer = 0;
        rules_.bonus_display_gate = add(rules_.bonus_display_gate, 1);
        session_.initials_initialized = 0;
        announce_record(44);
        grant_replays(1);
    } else
        rules_.bonus_display_gate = add(rules_.bonus_display_gate, 2);
}
bool RecordFlowRules::page_wait(RecordPage page, std::uint32_t edges) {
    if (effects_ && effects_->record_page_active(page) && !(edges & input_action::launch)) {
        replay_delay_ = rule_time::half_second;
        return true;
    }
    return false;
}
bool RecordFlowRules::complete_initials(std::uint32_t& edges, std::uint32_t& repeats) {
    if (!improved_records_)
        return entry_.step(edges, repeats);
    auto& saved = session_.players().at(unsigned(rules_.record_entry_player)).records.confirmed_initials;
    if (saved) {
        auto value = std::uint32_t(session_.initials_letters) & 0xff000000u;
        for (unsigned i = 0; i < saved->size(); ++i)
            value |= std::uint32_t((*saved)[i]) << (i * 8);
        session_.initials_letters = bits(value);
        return !replay_.awards;
    }
    if (!entry_.step(edges, repeats))
        return false;
    const auto value = std::uint32_t(session_.initials_letters);
    saved = std::array<std::uint8_t, 3>{std::uint8_t(value), std::uint8_t(value >> 8), std::uint8_t(value >> 16)};
    return true;
}
void RecordFlowRules::step(std::uint32_t& edges, std::uint32_t& repeats) {
    simulation_.mechanisms.enabled = 0;
    const auto current = rules_.bonus_display_gate;
    switch (current) {
        case 0:
            for (auto& player : session_.players())
                player.records.processed = 0;
            if (improved_records_) {
                for (auto& player : session_.players())
                    player.records.confirmed_initials.reset();
                for (auto& player : session_.players())
                    player.records.pre_buyin_processed = false;
                state_.entering_pre_buyin = false;
            }
            initialize_saved_initials(session_.players());
            rules_.bonus_display_gate = 1;
            return;
        case 1:
            find_champion(ChampionKind::combos, records_.combos(), RecordPage::combo);
            return;
        case 2:
            if (!page_wait(RecordPage::combo, edges) && complete_initials(edges, repeats)) {
                copy_initials(records_.combos().initials);
                rules_.bonus_display_gate = 3;
            }
            return;
        case 3:
            find_champion(ChampionKind::loops, records_.loops(), RecordPage::loop);
            return;
        case 4:
            if (!page_wait(RecordPage::loop, edges) && complete_initials(edges, repeats)) {
                copy_initials(records_.loops().initials);
                rules_.bonus_display_gate = 5;
            }
            return;
        case 5: {
            state_.entering_pre_buyin = false;
            rules_.record_best_high = 0;
            rules_.record_best_low = 0;
            rules_.record_scan_index = 0;
            rules_.record_best_player = -1;
            const auto count = rules_.player_count;
            if (count > 4)
                throw std::out_of_range("Invalid record player count");
            for (int i = 0; i < count; ++i) {
                const auto score = player_score(unsigned(i));
                if (!session_.players().at(unsigned(i)).records.processed && at_least(score, selected_score())) {
                    rules_.record_best_high = bits(score.rollovers);
                    rules_.record_best_low = bits(score.units);
                    rules_.record_best_player = i;
                }
                rules_.record_scan_index = i + 1;
            }
            const auto selected = rules_.record_best_player;
            if (selected == -1) {
                rules_.bonus_display_gate = 100;
                return;
            }
            session_.players().at(unsigned(selected)).records.processed = 1;
            const auto score = player_score(unsigned(selected));
            if (improved_records_ && player(unsigned(selected)).records.buyins && !session_.players().at(unsigned(selected)).records.pre_buyin_processed) {
                session_.players().at(unsigned(selected)).records.pre_buyin_processed = true;
                const auto& before = session_.players().at(unsigned(selected)).records.pre_buyin;
                if (before && find_rank(records_.ordinary(), *before, 0) >= 0) {
                    session_.players().at(unsigned(selected)).records.processed = 0;
                    state_.entering_pre_buyin = true;
                    rules_.record_best_low = bits(before->units);
                    rules_.record_best_high = bits(before->rollovers);
                    rules_.record_entry_player = selected;
                    insert_ordinary();
                    return;
                }
            }
            if (!player(unsigned(selected)).records.buyins) {
                if (find_rank(records_.ordinary(), score, 0) >= 0) {
                    rules_.record_entry_player = selected;
                    insert_ordinary();
                    return;
                }
            } else if (find_rank(records_.buyin(), score, 1) >= 0) {
                rules_.record_entry_player = selected;
                insert_buyin();
                return;
            }
            if (find_rank(records_.session(), score, 1) >= 0) {
                rules_.record_entry_player = selected;
                insert_session();
            }
            return;
        }
        case 6:
        case 7:
        case 8:
            if (page_wait(RecordPage::score, edges) || !complete_initials(edges, repeats))
                return;
            if (current == 6)
                copy_initials(records_.ordinary().at(unsigned(rules_.record_rank)).initials);
            else if (current == 7)
                copy_initials(records_.buyin().at(unsigned(rules_.record_rank)).initials);
            else
                copy_initials(records_.session().at(unsigned(rules_.record_rank)).initials);
            rules_.bonus_display_gate = 5;
            if (current != 8 && !state_.entering_pre_buyin)
                finish_session_entry();
            return;
        default:
            rules_.phase(GamePhase::match);
            return;
    }
}
} // namespace web::rules
