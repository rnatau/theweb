#include "bonus.hpp"
#include "drop_targets.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;

namespace S = web::rules::session_field;
using namespace physics::integer;
namespace {
constexpr int last_ball = 3, stage_total = 9, stage_end = 10, stage_continue = 11, stage_next = 12, stage_done = 13;
constexpr int stage_duration = rule_time::second, total_duration = 3 * rule_time::half_second;
constexpr int held_total_duration = 7 * rule_time::quarter_second, skipped_held_duration = 5 * rule_time::quarter_second, continue_duration = 10 * rule_time::second;
constexpr std::uint32_t flippers = 3, launch = 4, buyin = 0x10;
constexpr unsigned bonus_music_track = 18;
} // namespace
Player& BonusRules::current() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
Player& BonusRules::turn() {
    return session_.players().at(std::uint32_t(rules_.player_turn));
}
void BonusRules::finish_player() {
    auto& p = current();
    for (auto* mode : {&p.launcher.state, &p.combos.state, &p.quick_shot.state})
        if (*mode == mode_state::active)
            *mode = mode_state::inactive;
    const bool preserve_complete = !rules_.mission_time || session_.mission_kind != 0;
    for (auto first : {0u, DropTargetRules::targets_per_bank}) {
        const bool complete = p.targets.at(first) && p.targets.at(first + 1) && p.targets.at(first + 2);
        if (!preserve_complete || !complete)
            for (unsigned target = first; target < first + DropTargetRules::targets_per_bank; ++target)
                p.targets.at(target) = 0;
    }
    if (p.bonus.multiplier > 10)
        p.bonus.multiplier = 10;
    session_.dmd_animation = 0;
    session_.dmd_status = dmd_status::table;
    session_.dmd_frame = 0;
}
void BonusRules::begin() {
    rules_.phase(GamePhase::bonus);
    scoring_.stop_music();
    finish_player();
    rules_.bonus_timer = stage_duration;
    rules_.bonus_stage = rules_.tilted ? stage_end : -1;
    if (!rules_.tilted) {
        rules_.music_fade_timer = 0;
        scoring_.stop_sounds();
        scoring_.music_track(bonus_music_track);
    }
}
std::uint32_t BonusRules::total(std::uint32_t scale) const {
    const auto& p = session_.players().at(unsigned(rules_.current_player));
    // Preserve the original unsigned multiplication and division order.
    const auto base = std::uint32_t(p.bonus.loops) * (40000u * scale) + std::uint32_t(p.bonus.power_levels) * (50000u * scale) + std::uint32_t(p.bonus.combos) * (60000u * scale);
    return base * std::uint32_t(p.bonus.multiplier);
}
void BonusRules::award(bool skipped) {
    const bool held = current().bonus.hold != 0;
    rules_.bonus_timer = skipped ? (held ? skipped_held_duration : stage_duration) : (held ? held_total_duration : total_duration);
    scoring_.add(total(ScoreValue::points_per_unit) / ScoreValue::points_per_unit);
}
void BonusRules::continue_or_advance(std::uint32_t& edges, std::uint32_t held) {
    auto& p = turn();
    auto stage = rules_.bonus_stage;
    if (stage == stage_end) {
        rules_.extra_ball_turn = 0;
        if (p.ball.extra_balls) {
            rules_.extra_ball_turn = 1;
            rules_.phase(GamePhase::begin_ball);
            p.ball.extra_balls = sub(p.ball.extra_balls, 1);
        } else {
            rules_.bonus_stage = stage_continue;
            rules_.continue_timer = continue_duration;
        }
    }
    if (rules_.bonus_stage == stage_continue) {
        auto& credits = scoring_.replay().credits;
        if (p.ball.number == last_ball && credits) {
            if (rules_.continue_timer == continue_duration)
                scoring_.sound({102, -64, 0, 1});
            const auto remaining = std::max(0, sub(rules_.continue_timer, rules_.elapsed));
            rules_.continue_timer = remaining;
            display_.continue_seconds = remaining / stage_duration;
            if (edges & buyin) {
                if (!p.records.buyins && first_buyin_)
                    first_buyin_(unsigned(rules_.player_turn), {std::uint32_t(p.score.units), std::uint32_t(p.score.rollovers)});
                p.records.buyins = add(p.records.buyins, 1);
                rules_.extra_ball_turn = 1;
                credits = sub(credits, 1);
                rules_.phase(GamePhase::begin_ball);
            } else {
                if ((edges & flippers) && (held & flippers) == flippers)
                    rules_.bonus_stage = stage_next;
                if (!remaining || (edges & input_action::confirm)) {
                    edges = 0;
                    rules_.bonus_stage = stage_next;
                }
            }
        } else
            rules_.bonus_stage = stage_next;
    }
    display_.stage = rules_.bonus_stage;
    if (display_.stage == stage_next) {
        rules_.bonus_stage = stage_done;
        p.ball.number = add(p.ball.number, 1);
        auto next = add(rules_.player_turn, 1);
        if (next == rules_.player_count)
            next = 0;
        rules_.player_turn = next;
        rules_.phase(GamePhase::begin_ball);
        if (turn().ball.number == last_ball + 1)
            rules_.phase(GamePhase::records);
    }
}
void BonusRules::step(std::uint32_t& edges, std::uint32_t held) {
    rules_.mission_time = 0;
    auto& p = current();
    if (p.ball.number == last_ball && !p.ball.extra_balls && !scoring_.replay().credits)
        p.bonus.hold = 0;
    const auto remaining = std::max(0, sub(rules_.bonus_timer, rules_.elapsed));
    rules_.bonus_timer = remaining;
    if (!remaining && rules_.bonus_stage < stage_end) {
        const auto next = add(rules_.bonus_stage, 1);
        rules_.bonus_stage = next;
        rules_.bonus_timer = stage_duration;
        if (next == stage_total)
            award(false);
    }
    if (rules_.bonus_stage < stage_total && (edges & (flippers | launch)) && ((held & flippers) == flippers || (edges & launch))) {
        scoring_.stop_music();
        rules_.bonus_stage = stage_total;
        award(true);
    }
    switch (rules_.bonus_stage) {
        case 1:
            display_.value = std::uint32_t(p.bonus.loops) * 40000u;
            break;
        case 3:
            display_.value = std::uint32_t(p.bonus.power_levels) * 50000u;
            break;
        case 5:
            display_.value = std::uint32_t(p.bonus.combos) * 60000u;
            break;
        case stage_total:
            display_.value = total(1);
            break;
    }
    continue_or_advance(edges, held);
    rules_.bonus_display_gate = 0;
}
} // namespace web::rules
