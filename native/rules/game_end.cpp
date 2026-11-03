#include "game_end.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr auto index = F::record_scan_index;
} // namespace
void GameEndRules::feedback() {
    replay_delay_ = std::max(0, sub(replay_delay_, rules_.elapsed));
    if (!replay_delay_ && replay_.awards) {
        replay_.awards = sub(replay_.awards, 1);
        replay_delay_ = rule_time::three_quarter_second;
        if (sounds_) {
            sounds_->sound({192, 0, 0, 18});
            sounds_->sound({192, 0, 0, 17});
        }
    }
}
void GameEndRules::random_digit() {
    do {
        rules_.match_digit = int(simulation_.random.next() & 15);
    } while (rules_.match_digit > 9);
}
void GameEndRules::match() {
    const auto stage = rules_.match_stage;
    if (stage == 0) {
        rules_.match_digit = int(simulation_.random.next() & 15);
        rules_.match_stage = 1;
        if (effects_) {
            effects_->end_clear_page();
            effects_->match_page(128, 11);
        }
    } else if (stage == 1) {
        random_digit();
        rules_.match_timer = std::max(0, sub(rules_.match_timer, rules_.elapsed));
        if (rules_.match_timer)
            return;
        rules_.match_stage = 2;
        rules_.match_timer = (2 * rule_time::second);
        const auto count = rules_.player_count;
        if (count > 4)
            throw std::out_of_range("Invalid match player count");
        // Preserve the original retry and signed awarded flag, including its
        // -1 sentinel becoming truthy on a subsequent selection.
        do {
            random_digit();
            for (int i = 0; i < count; ++i) {
                const auto value = std::uint32_t(session_.players().at(unsigned(i)).score.units);
                if (value % 10 == unsigned(rules_.match_digit)) {
                    replay_.awards = add(replay_.awards, 1);
                    replay_.credits = std::min(9, add(replay_.credits, 1));
                    rules_.match_timer = (4 * rule_time::second);
                    rules_.match_awarded = 1;
                    if (sounds_)
                        sounds_->sound({18, 64, 0, 1});
                }
                // Original scratch aliases the player's loop index. The final
                // value is the count, including zero/negative-count fixtures.
                rules_.record_scan_index = i;
            }
            rules_.record_scan_index = std::max(0, count);
            rules_.match_awarded = rules_.match_awarded ? 1 : -1;
            if (replay_.threshold_rollovers)
                rules_.match_awarded = 1;
        } while (rules_.match_awarded != 1);
    } else if (stage == 2) {
        rules_.match_timer = std::max(0, sub(rules_.match_timer, rules_.elapsed));
        if (!rules_.match_timer) {
            rules_.phase(GamePhase::finish);
            if (effects_)
                effects_->end_clear_page();
        }
    }
}
void GameEndRules::finish() {
    const auto count = rules_.player_count;
    if (count > 4)
        throw std::out_of_range("Invalid finished player count");
    auto low = bits(replay_.threshold_units), high = replay_.threshold_rollovers;
    for (int i = 0; i < count; ++i) {
        if (session_.players().at(unsigned(i)).records.replay_awarded) {
            low = add(low, 50000000);
            if (low >= 100000000) {
                low = sub(low, 100000000);
                high = add(high, 1);
            }
        } else {
            low = sub(low, 10000000);
            if (high) {
                if (low < 0) {
                    low = add(low, 100000000);
                    high = sub(high, 1);
                }
            } else if (low < 50000000)
                low = 50000000;
        }
    }
    state_.previous_players = count;
    simulation_.mechanisms.enabled = 0;
    rules_.start_latch = 0;
    rules_.phase(GamePhase::attract);
    rules_.player_turn = 0;
    rules_.player_count = 0;
    session_.players().at(0).ball.number = 0;
    replay_.threshold_units = std::uint32_t(low);
    replay_.threshold_rollovers = high;
    if (effects_)
        effects_->end_stop_animations();
    if (session_.music_track != 12) {
        rules_.music_fade_timer = 0;
        session_.request_music_track(19); // Original 0x415834 bookkeeping; playback is optional.
        if (sounds_) {
            sounds_->stop();
            sounds_->sound({129, -64, 0, 1});
        }
    }
}
void GameEndRules::step(std::uint32_t& edges, std::uint32_t& repeats) {
    switch (rules_.phase()) {
        case GamePhase::records:
            records_.step(edges, repeats);
            rules_.match_stage = 0;
            rules_.match_awarded = 0;
            rules_.match_timer = (2 * rule_time::second);
            break;
        case GamePhase::match:
            match();
            break;
        case GamePhase::finish:
            finish();
            break;
        default:
            break;
    }
}
} // namespace web::rules
