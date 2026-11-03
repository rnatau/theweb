#include "power_levels.hpp"
#include "lamps.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;

namespace S = web::rules::session_field;
using namespace physics::integer;
namespace {
constexpr int short_flash = rule_time::quarter_second, award_flash = rule_time::second;
constexpr std::uint32_t level_value = 100000, first_level_award = 500000, sphere_award = 50;
constexpr auto jet_gate_timer = S::jet_gate_timer, jet_completion_window = S::jet_completion_window;
constexpr int time_unit = rule_time::second, jet_window_duration = 30 * time_unit;
} // namespace
Player& PowerLevelRules::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void PowerLevelRules::advance_superjets() {
    auto& p = player();
    if (p.bumpers.super_jets != 3) {
        p.bumpers.super_jets = add(p.bumpers.super_jets, 1);
        const auto jets = p.bumpers.super_jets;
        if (jets >= 1 && jets <= 3)
            scoring_.sound({jets == 1 ? 60 : 6, 64, 0, 1});
    }
}
void PowerLevelRules::ready_quick_shot() {
    auto& p = player();
    p.quick_shot.count = add(p.quick_shot.count, 1);
    if (p.quick_shot.state != 2) {
        scoring_.sound({117, -64, 0, 1});
        p.quick_shot.state = 1;
        LampRules{}.queue(rules_, session_, TableLamp::quick_shot, award_flash);
    }
}
void PowerLevelRules::step_jets() {
    const auto decrement = [&](web::rules::SessionMember field) {
        if (const auto value = (session_.*field))
            (session_.*field) = std::max(0, sub(value, rules_.elapsed));
    };
    if (rules_.switch_edge(TableSwitch::jet_sequence_gate) && player().bumpers.super_jets != 3)
        session_.jet_gate_timer = time_unit;
    if (session_.jet_gate_timer) {
        decrement(jet_gate_timer);
        if (rules_.switch_edge(TableSwitch::bumper_a) || rules_.switch_edge(TableSwitch::bumper_b) || rules_.switch_edge(TableSwitch::bumper_c)) {
            session_.jet_gate_timer = 0;
            if (session_.jet_completion_window) {
                session_.jet_completion_window = 0;
                advance_superjets();
            } else if (!rules_.drain_override)
                session_.jet_completion_window = jet_window_duration;
        }
    }
    decrement(jet_completion_window);
}
void PowerLevelRules::advance() {
    LampRules lights;
    auto& p = player();
    lights.queue(rules_, session_, TableLamp::power_level, short_flash);
    p.bonus.power_levels = add(p.bonus.power_levels, 1);
    const auto level = add(p.power.level, 1);
    p.power.level = level;
    if (effects_)
        effects_->power_level(level, !rules_.drain_override || p.lock.state == mode_state::active);
    scoring_.add(std::uint32_t(level) * level_value);
    if (level >= 1 && level <= 8 && level != 3 && level != 4) {
        constexpr int samples[] = {107, 55, 108, 56, 109, 57, 110, 58};
        scoring_.sound({samples[unsigned(level - 1)], level & 1 ? -64 : 64, 0, 1});
    }
    switch (level) {
        case 1:
            scoring_.add(first_level_award);
            break;
        case 2:
            p.awards.explosion_ready = 1;
            lights.queue(rules_, session_, TableLamp::explosion, award_flash);
            break;
        case 3:
            advance_superjets();
            scoring_.sound({108, -64, 0, 1});
            break;
        case 4:
            ready_quick_shot();
            scoring_.sound({56, 64, 0, 1});
            break;
        case 5:
            lights.queue(rules_, session_, TableLamp::video_mode, award_flash);
            p.video.ready = 1;
            break;
        case 6:
            lights.queue(rules_, session_, TableLamp::kickback, award_flash);
            p.kickback.available = 1;
            rules_.super_rescue = 1;
            break;
        case 7:
            p.launcher.state = mode_state::ready;
            lights.queue(rules_, session_, TableLamp::super_launcher, award_flash);
            break;
        case 8:
            if (!p.power.sphere)
                p.power.sphere = 1;
            p.showdown.spheres = add(p.showdown.spheres, 1);
            scoring_.hundred_thousands(sphere_award);
            break;
    }
}
} // namespace web::rules
