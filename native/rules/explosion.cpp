#include "explosion.hpp"
#include "table_mechanisms.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;

using namespace physics::integer;
namespace {
constexpr int time_unit = rule_time::second;
} // namespace
Player& ExplosionRules::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void ExplosionRules::animation(unsigned id, std::uint32_t flags) {
    if (awards_)
        awards_->award_animation(id, flags);
}
void ExplosionRules::page(ExplosionPage page) {
    if (effects_)
        effects_->explosion_page(page, 18, -1);
}
bool ExplosionRules::apply(unsigned choice) {
    auto& p = player();
    LampRules lamps;
    if (choice >= 1 && choice <= 3) {
        if (p.power.level == 8)
            return false;
        power_.advance();
    } else if (choice >= 4 && choice <= 6) {
        if (p.bumpers.super_jets == 3)
            return false;
        power_.advance_superjets();
        page(ExplosionPage::superjets);
    } else if (choice >= 7 && choice <= 12) {
        auto& held = choice < 10 ? p.bumpers.hold : p.bonus.hold;
        if (held)
            return false;
        held = 1;
        animation(choice < 10 ? 4 : 3, 1);
        scoring_.sound({choice < 10 ? 6 : 61, 64, 0, 1});
    } else if (choice >= 13 && choice <= 15) {
        if (p.magnet.hold)
            return false;
        scoring_.sound({123, -64, 0, 1});
        p.magnet.hold = 1;
        lamps.queue(rules_, session_, 39, time_unit);
        if (!rules_.drain_override)
            animation(5, 0);
    } else if (choice >= 16 && choice <= 18)
        scoring_.advance_bonus();
    else
        switch (choice) {
            case 0:
                if (rules_.save_timer)
                    return false;
                rules_.save_timer = 30 * time_unit;
                lamps.queue(rules_, session_, 1, time_unit);
                page(ExplosionPage::saver);
                break;
            case 19:
                scoring_.sound({171, 0, 0, 18});
                scoring_.sound({171, 0, 0, 17});
                rules_.flipper_timing_window = (3 * rule_time::half_second);
                break;
            case 20:
                if (p.kickback.available)
                    return false;
                TableMechanismRules::restore_kickback(rules_, session_, &scoring_, [this](unsigned id, std::uint32_t flags) { animation(id, flags); });
                break;
            case 21:
                if (p.quick_shot.state)
                    return false;
                power_.ready_quick_shot();
                page(ExplosionPage::quick_shot);
                break;
            case 22:
                if (rules_.magnet_activation_window || p.magnet.hold)
                    return false;
                video_.start();
                break;
            case 23:
                if (p.video.ready)
                    return false;
                scoring_.sound({119, -64, 0, 1});
                p.video.ready = 1;
                lamps.queue(rules_, session_, TableLamp::video_mode, time_unit);
                page(ExplosionPage::video_ready);
                break;
            case 24:
                if (p.station.state)
                    return false;
                animation(15, 1);
                scoring_.sound({122, -64, 0, 1});
                p.station.state = mode_state::ready;
                lamps.queue(rules_, session_, 43, time_unit);
                break;
            case 25:
                if (rules_.outlane_extra_ball)
                    return false;
                rules_.outlane_extra_ball = 1;
                lamps.queue(rules_, session_, 15, time_unit);
                lamps.queue(rules_, session_, 19, time_unit);
                page(ExplosionPage::outlanes);
                break;
            case 26:
                scoring_.replay().credits = std::min(9, add(scoring_.replay().credits, 1));
                scoring_.replay().awards = add(scoring_.replay().awards, 1);
                state_.replay_delay = time_unit;
                animation(1, 1);
                break;
            case 27:
                if (p.lock.state)
                    return false;
                p.lock.state = mode_state::ready;
                p.lock.balls = 3;
                p.lock.progress = 3;
                locks_.start();
                break;
            case 28:
                if (p.lock.state)
                    return false;
                ramps_.light_lock();
                break;
            case 29:
                if (rules_.super_rescue)
                    return false;
                lamps.queue(rules_, session_, TableLamp::kickback, time_unit);
                p.kickback.available = 1;
                rules_.super_rescue = 1;
                page(ExplosionPage::super_rescue);
                break;
            case 30:
                if (p.awards.extra_ball_awards)
                    return false;
                scoring_.sound({126, -64, 0, 1});
                lamps.queue(rules_, session_, TableLamp::extra_ball_lit, time_unit);
                p.awards.extra_balls_lit = add(p.awards.extra_balls_lit, 1);
                animation(33, 1);
                break;
            case 31:
                if (p.awards.extra_ball_awards)
                    return false;
                extra_.award();
                break;
        }
    return true;
}
void ExplosionRules::step() {
    rules_.explosion_wait = std::max(0, sub(rules_.explosion_wait, rules_.elapsed));
    if (rules_.explosion_wait)
        return;
    rules_.returns_paused = 1;
    for (;;) {
        auto choice = simulation_.random.next() & 31;
        auto& p = player();
        if (p.ball.number == 3 && !p.ball.extra_balls && !p.score.rollovers && std::uint32_t(p.score.units) < 10000000) {
            choice = 22;
            rules_.magnet_activation_window = 0;
            p.magnet.hold = 0;
        }
        if (apply(choice))
            break;
    }
    rules_.returns_paused = 0;
    auto& p = player();
    if (p.awards.explosion_ready == 2)
        p.awards.explosion_ready = 0;
    if (rules_.magnet_activation_window) {
        rules_.magnet_activation_window = 0;
        rules_.magnet_combo_timer = (6 * rule_time::second);
    } else if (!p.magnet.hold)
        return;
    simulation_.force_timer = (5 * rule_time::second);
    rules_.magnet_voice_timer = rules_.super_launcher_timer ? 0x15999 : 0x19999;
    p.magnet.hold = 0;
}
void ExplosionRules::timing_shot(std::uint32_t edges, std::uint32_t held) {
    if (!rules_.flipper_timing_window)
        return;
    rules_.flipper_timing_window = std::max(0, sub(rules_.flipper_timing_window, rules_.elapsed));
    if (!(edges & input_action::both_flippers) || (held & input_action::both_flippers) != input_action::both_flippers)
        return;
    const auto factor = sub((3 * rule_time::half_second), rules_.flipper_timing_window) / 150;
    const auto units = std::uint32_t(mul(mul(factor, factor), factor));
    if (awards_)
        awards_->award_collect(ScoringPage::flashing_award, 18, 1, units);
    scoring_.add(units);
    rules_.flipper_timing_window = 0;
}
} // namespace web::rules
