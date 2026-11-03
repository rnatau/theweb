#include "scoring.hpp"
#include "lamps.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;

namespace S = web::rules::session_field;
using Switch = TableSwitch;
using namespace physics::integer;
namespace {
constexpr std::uint32_t hundred = 100, hundred_thousand = 100000;
constexpr std::uint32_t sling_value = 1110, bumper_value = 3700, spinner_value = 1107;
constexpr std::int32_t spinner_level_value = 10000, bumper_accumulator = 7130;
constexpr std::int32_t ramp_increment = 1000000, skillshot_increment = 25000000;
constexpr std::int32_t short_flash = rule_time::quarter_second, award_flash = rule_time::second;
constexpr unsigned replay_animation = 2, bonus_twice_animation = 35, bonus_four_animation = 36, bonus_six_animation = 37, bonus_eight_animation = 38;
constexpr unsigned extra_ball_lit_animation = 33, bonus_max_animation = 39, bonus_score_animation = 40;
constexpr unsigned contact_page_ticks = 24, bonus_score_ticks = 9;
} // namespace
std::uint64_t ScoreValue::points() const {
    return (std::uint64_t(rollovers) * radix + units) * points_per_unit;
}
std::string ScoreValue::text() const {
    auto result = std::to_string(points());
    for (auto position = std::int64_t(result.size()) - 3; position > 0; position -= 3)
        result.insert(std::size_t(position), ",");
    return result;
}
Player& ScoringRules::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void ScoringRules::animation(unsigned id, std::uint32_t flags) {
    if (effects_)
        effects_->scoring_animation(id, flags);
}
void ScoringRules::page(ScoringPage kind, unsigned ticks, int priority, bool replace) {
    if (effects_)
        effects_->scoring_page(kind, ticks, priority, replace);
}
bool ScoringRules::show_contact_page() const {
    return !rules_.mission_time && !rules_.drain_override && rules_.single_ball_award_mode != 2 && effects_ && !effects_->scoring_award_page_active();
}
ScoreValue ScoringRules::score() const {
    const auto index = std::uint32_t(rules_.current_player);
    return {std::uint32_t(session_.players().at(index).score.units), std::uint32_t(session_.players().at(index).score.rollovers)};
}
void ScoringRules::add(std::uint32_t units) {
    auto& p = player();
    p.score.add_units(units);
    const auto high = p.score.rollovers;
    const auto low = std::uint32_t(p.score.units);
    if (p.records.replay_awarded == 0 && (high > replay_.threshold_rollovers || (high == replay_.threshold_rollovers && low >= replay_.threshold_units))) {
        sound({19, 64, 0, 1});
        if (!rules_.video_active)
            animation(replay_animation);
        replay_.awards = physics::integer::add(replay_.awards, 1);
        replay_.credits = std::min(physics::integer::add(replay_.credits, 1), 9);
        p.records.replay_awarded = 1;
    }
}
void ScoringRules::tenths(std::uint32_t units) {
    add(units / ScoreValue::points_per_unit);
}
void ScoringRules::hundreds(std::uint32_t units) {
    add(units * hundred);
}
void ScoringRules::hundred_thousands(std::uint32_t units) {
    add(units * hundred_thousand);
}
void ScoringRules::spinner(unsigned index) {
    sound({175, index ? 64 : -64, 0, 17});
    const auto level = player().power.level;
    add(level ? std::uint32_t(mul(level, spinner_level_value)) : spinner_value);
}
void ScoringRules::music_track(unsigned track) {
    session_.request_music_track(bits(track));
}
void ScoringRules::stop_music() {
    session_.request_music_track(-1);
}
void ScoringRules::announce() {
    auto choice = std::int32_t((rules_.frame_phase() >> 4) & 7);
    if (choice == rules_.announcement_voice)
        choice = physics::integer::add(choice, 1) & 7;
    rules_.announcement_voice = choice;
    constexpr int samples[] = {19, 15, 16, 17, 18, 39, 14, 40};
    sound({samples[unsigned(choice)], 64, 0, 1});
}
void ScoringRules::fill_next_rollover() {
    auto& p = player();
    for (unsigned i = 0; i < 3; ++i)
        if (!p.rollovers[i]) {
            p.rollovers[i] = 1;
            LampRules{}.queue(rules_, session_, 62 + i, short_flash);
            return;
        }
}
void ScoringRules::slings() {
    for (auto contact : {Switch::sling_a, Switch::sling_b})
        if (rules_.switch_edge(contact))
            tenths(sling_value);
}
void ScoringRules::bumpers() {
    for (auto contact : {Switch::bumper_a, Switch::bumper_c, Switch::bumper_b})
        if (rules_.switch_edge(contact)) {
            auto& p = player();
            p.awards.ramp_value = physics::integer::add(p.awards.ramp_value, bumper_accumulator);
            const auto jets = p.bumpers.super_jets;
            const bool ammo_dump = rules_.mission_time && session_.mission_kind == 2;
            if (jets || ammo_dump)
                sound({ammo_dump ? 176 : 180, 0, ammo_dump ? 64 : 96, 17});
            add(jets ? std::uint32_t(mul(jets, std::int32_t(hundred_thousand))) : bumper_value);
            if (jets)
                rules_.super_jet_total = physics::integer::add(rules_.super_jet_total, jets);
            if (show_contact_page())
                page(jets ? ScoringPage::superjet_total : ScoringPage::ramp_value, contact_page_ticks, 0, false);
        }
}
void ScoringRules::ramp() {
    auto& p = player();
    if (rules_.launch_timer) {
        rules_.launch_timer = 0;
        const auto value = std::uint32_t(p.awards.skillshot_value);
        add((rules_.super_skillshot ? value * 2u : value) / ScoreValue::points_per_unit);
        if (effects_)
            effects_->scoring_skillshot((value / ScoreValue::points_per_unit) * (rules_.super_skillshot ? 2u : 1u));
        page(ScoringPage::skillshot, contact_page_ticks, -1, true);
        p.awards.skillshot_value = physics::integer::add(p.awards.skillshot_value, skillshot_increment);
        sound({rules_.super_skillshot ? 15 : 14, 64, 0, 1});
    }
    p.awards.ramp_value = physics::integer::add(p.awards.ramp_value, ramp_increment);
    if (p.station.state != mode_state::ready && p.station.state != mode_state::active && !rules_.magnet_combo_timer && !rules_.station_ramp_ready &&
        rules_.station_super_stage() == StationSuperStage::inactive && p.launcher.state != mode_state::active && show_contact_page())
        page(ScoringPage::ramp_value, contact_page_ticks, 0, false);
}
void ScoringRules::advance_bonus() {
    sound({169, 0, 32, 2});
    LampRules{}.queue(rules_, session_, 8, short_flash);
    auto& p = player();
    const auto before = p.bonus.multiplier;
    const auto after = before == 1 ? 2 : physics::integer::add(before, 2);
    p.bonus.multiplier = after;
    if (before == 1 || after == 4 || after == 6 || after == 8) {
        animation(before == 1 ? bonus_twice_animation : after == 4 ? bonus_four_animation : after == 6 ? bonus_six_animation : bonus_eight_animation);
        return;
    }
    if (after == 10) {
        if (!p.bonus.maxed) {
            p.bonus.maxed = 1;
            sound({126, -64, 0, 1});
            LampRules{}.queue(rules_, session_, 54, award_flash);
            p.awards.extra_balls_lit = physics::integer::add(p.awards.extra_balls_lit, 1);
            animation(extra_ball_lit_animation);
        }
        animation(bonus_max_animation, 0);
        return;
    }
    if (!p.bonus.sphere)
        p.bonus.sphere = 1;
    const auto amount = std::uint32_t(physics::integer::add(mul(sub(after / 2, 5), 5), 5)) * hundred_thousand;
    add(amount);
    animation(bonus_score_animation);
    page(ScoringPage::award, bonus_score_ticks, -1, true);
    if (effects_)
        effects_->scoring_amount({amount % ScoreValue::radix, (amount / ScoreValue::radix) % 10});
}
} // namespace web::rules
