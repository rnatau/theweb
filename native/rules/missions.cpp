#include "missions.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using Switch = TableSwitch;
using namespace physics::integer;
namespace mission_animation {
// Descriptive recovery labels for original DMS asset slots, not memory addresses.
constexpr unsigned reactor_active = 41;
constexpr unsigned reactor_timeout = 42;
constexpr unsigned reactor_complete = 43;
constexpr unsigned hunt_intro = 44;
constexpr unsigned hunt_hit = 45;
constexpr unsigned hunt_active = 46;
constexpr unsigned hunt_complete = 47;
constexpr unsigned ammo_stage_base = 48;
constexpr unsigned ammo_next_stage_base = 49;
constexpr unsigned ammo_complete = 51;
constexpr unsigned skyscraper_intro = 52;
constexpr unsigned skyscraper_climb = 53;
constexpr unsigned skyscraper_floor_award = 54;
constexpr unsigned skyscraper_collect = 55;
constexpr unsigned skyscraper_active = 56;
constexpr unsigned skyscraper_award = 57;
constexpr unsigned shuttle_active = 58;
constexpr unsigned shuttle_ramp_hit = 59;
constexpr unsigned shuttle_complete = 60;
constexpr unsigned shuttle_timeout = 61;
constexpr unsigned shuttle_centre_hit = 62;
constexpr unsigned biker_active = 63;
constexpr unsigned biker_hit_a = 64;
constexpr unsigned biker_hit_b = 65;
constexpr unsigned biker_timeout = 66;
constexpr unsigned biker_complete = 67;
constexpr unsigned sphere = 125;
} // namespace mission_animation
namespace {
constexpr unsigned mission_count = unsigned(Mission::count);
constexpr int time_unit = rule_time::second, short_flash = rule_time::quarter_second, award_flash = time_unit;
constexpr int bank_a_reset = DropTargetRules::bank_a_reset, bank_b_reset = DropTargetRules::bank_b_reset;
constexpr int completion_award = 50;
constexpr std::array<unsigned, 6> mission_music_tracks = {9, 6, 5, 8, 4, 7};
constexpr int slow_blink = 8 * rule_time::second, fast_blink = 16 * rule_time::second;
constexpr std::uint32_t replace_animation = 1, mirror_animation = 2, hold_animation = 4;
constexpr unsigned intro_ticks = 42, result_ticks = 42, short_result_ticks = 24;
constexpr Switch centre_contact = Switch::centre_shot, sinkhole_contact = Switch::centre_sinkhole, reactor_collect_contact = Switch::upper_sinkhole;
void decrement(TurnState& r, web::rules::TurnMember field) {
    if (const auto before = (r.*field))
        (r.*field) = std::max(0, sub(before, r.elapsed));
}
void decrement(TurnState& r, SharedGameState& s, web::rules::SessionMember field) {
    if (const auto before = (s.*field))
        (s.*field) = std::max(0, sub(before, r.elapsed));
}
} // namespace
Player& MissionRules::player() {
    return session_.players().at(std::uint32_t(rules_.current_player));
}
void MissionRules::animation(unsigned id, std::uint32_t flags, unsigned repeats) {
    if (effects_)
        effects_->mission_animation(id, flags, repeats);
}
void MissionRules::page(MissionPage kind, unsigned ticks, int priority, bool replace) {
    if (effects_)
        effects_->mission_page(kind, ticks, priority, replace);
}
void MissionRules::progress(int percent) {
    page(MissionPage::progress, 2, -1, false);
    if (effects_)
        effects_->mission_progress(percent, std::max(0, rules_.mission_time / time_unit - 2));
}
std::string MissionRules::name(Mission mission) {
    constexpr const char* names[] = {"Reactor Critical", "Hunt Down", "Destroy Ammodump", "Sky Scraper Raid", "Stop the Shuttle", "Biker Challenge"};
    return unsigned(mission) < mission_count ? names[unsigned(mission)] : "";
}
void MissionRules::advance_selection() {
    auto& p = player();
    if (p.missions.all_played)
        return;
    auto selected = p.missions.selected;
    for (unsigned attempt = 0; attempt < mission_count; ++attempt) {
        selected = add(selected, 1);
        if (selected > int(mission_count) - 1)
            selected = 0;
        if (selected < 0)
            throw std::invalid_argument("Negative original mission selection");
        if (!p.missions.played.at(unsigned(selected))) {
            p.missions.selected = selected;
            return;
        }
    }
    if (selection_fallback_) {
        selection_fallback_(p);
        return;
    }
    throw std::runtime_error("Original mission selection would loop over six played missions");
}
void MissionRules::award(std::int32_t factor) {
    const auto units = mul(add(player().missions.rounds, 1), factor);
    scoring_.hundred_thousands(std::uint32_t(units));
    rules_.mission_total = add(rules_.mission_total, units);
}
void MissionRules::complete() {
    auto& p = player();
    p.showdown.spheres = add(p.showdown.spheres, 1);
    animation(mission_animation::sphere);
    scoring_.hundred_thousands(completion_award);
}
void MissionRules::finish() {
    session_.mission_centre_lit = 0;
    rules_.mission_time = 0;
    scoring_.stop_music();
    scoring_.resume_music();
    // Original target-bank reset helpers 0x404990 / 0x40499c.
    rules_.target_bank_a_timer = bank_a_reset;
    rules_.target_bank_b_timer = bank_b_reset;
}
void MissionRules::countdown() {
    if (rules_.mission_time) {
        decrement(rules_, F::mission_time);
        rules_.mission_intro = -1;
    }
    if (!rules_.mission_time) {
        const auto kind = Mission(session_.mission_kind);
        if (kind == Mission::reactor)
            animation(mission_animation::reactor_timeout, replace_animation);
        else if (kind == Mission::shuttle)
            animation(mission_animation::shuttle_timeout, replace_animation);
        else if (kind == Mission::biker)
            animation(mission_animation::biker_timeout, replace_animation);
        else if (effects_)
            effects_->mission_stop_animation();
        finish();
        page(MissionPage::total, kind == Mission::hunt || kind == Mission::ammo_dump || kind == Mission::skyscraper ? short_result_ticks : result_ticks, -1);
    }
}
void MissionRules::start() {
    auto& p = player();
    LampRules lights;
    lights.queue(rules_, session_, 13, short_flash);
    lights.queue(rules_, session_, 53, award_flash);
    rules_.mission_intro = 5;
    rules_.drain_extension = 0;
    rules_.mission_total = 0;
    if (p.missions.all_played) {
        p.missions.rounds = add(p.missions.rounds, 1);
        page(MissionPage::cycle_complete, 36, 1);
        constexpr int mission_cycle_award = 250;
        award(mission_cycle_award);
        scoring_.sound({18, 64, 0, 1});
        complete();
        for (unsigned i = 0; i < mission_count; ++i)
            p.missions.played.at(i) = 0;
        if (!p.missions.sphere) {
            lights.queue(rules_, session_, 13, award_flash);
            p.missions.sphere = 1;
        }
        p.missions.all_played = 0;
        advance_selection();
        return;
    }
    const auto selected = unsigned(p.missions.selected);
    if (selected >= mission_count)
        throw std::invalid_argument("Original mission selection outside six missions");
    session_.mission_kind = int(selected);
    rules_.music_fade_timer = 0;
    scoring_.stop_sounds();
    scoring_.music_track(mission_music_tracks[selected]);
    constexpr std::array<MissionPage, 6> introductions =
        {MissionPage::reactor_intro, MissionPage::hunt_intro, MissionPage::ammo_intro, MissionPage::skyscraper_intro, MissionPage::shuttle_intro, MissionPage::biker_intro};
    page(introductions[selected], intro_ticks, 1);
    switch (Mission(selected)) {
        case Mission::reactor:
            rules_.mission_time = 50 * time_unit;
            animation(mission_animation::reactor_active, replace_animation);
            DropTargetRules{}.clear_progress(rules_, session_);
            session_.reactor_remaining = 10;
            session_.reactor_regeneration = 1;
            session_.mission_centre_lit = 1;
            break;
        case Mission::hunt:
            rules_.mission_time = 64 * time_unit;
            session_.hunt_remaining = 5;
            session_.hunt_target = int(random_.next() & 3);
            session_.mission_centre_lit = 1;
            rules_.mission_flash = 1;
            rules_.mission_flash_time = 2 * time_unit;
            rules_.return_toggle = 0;
            animation(mission_animation::hunt_intro, replace_animation);
            animation(mission_animation::hunt_active);
            break;
        case Mission::ammo_dump:
            rules_.mission_time = 50 * time_unit;
            session_.ammo_remaining = 25;
            session_.ammo_stage = 0;
            session_.ammo_strength = 0;
            session_.mission_centre_lit = 1;
            animation(mission_animation::ammo_stage_base, replace_animation | hold_animation);
            break;
        case Mission::skyscraper:
            rules_.mission_time = 63 * time_unit;
            rules_.skyscraper_floor = 0;
            session_.skyscraper_cooldown = 0;
            (session_.skyscraper_reset = 0); // Original Skyscraper reset word; further role unconfirmed.
            session_.skyscraper_extended = 0;
            session_.mission_centre_lit = 0;
            rules_.return_toggle = 0;
            animation(mission_animation::skyscraper_intro, replace_animation);
            animation(mission_animation::skyscraper_active);
            break;
        case Mission::shuttle:
            rules_.mission_time = 50 * time_unit;
            session_.mission_centre_lit = 0;
            session_.shuttle_hits = 0;
            session_.shuttle_charge = 4;
            rules_.return_toggle = 1;
            animation(mission_animation::shuttle_active, replace_animation);
            break;
        case Mission::biker:
            rules_.mission_time = 65 * time_unit;
            state_.biker_award = 5;
            state_.biker_timer = 6 * time_unit;
            state_.biker_alternate = 1;
            state_.biker_remaining = 4;
            state_.biker_shot = 0;
            session_.mission_centre_lit = 1;
            rules_.return_toggle = 1;
            animation(mission_animation::biker_active, replace_animation);
            break;
        case Mission::count:
            break;
    }
    scoring_.sound({59, 64, 0, 1});
    rules_.mission_flash = 0;
    p.missions.played.at(selected) = 1;
    int played = 0;
    for (unsigned i = 0; i < mission_count; ++i)
        played = add(played, p.missions.played.at(i));
    if (played == int(mission_count))
        p.missions.all_played = 1;
    advance_selection();
}
void MissionRules::reactor() {
    if (effects_ && !effects_->mission_animation_active())
        animation(mission_animation::reactor_active);
    auto& p = player();
    rules_.return_toggle = 0;
    if (session_.mission_centre_lit && rules_.switch_edge(centre_contact)) {
        session_.mission_centre_lit = 0;
        session_.reactor_regeneration = 0;
    }
    for (unsigned i = 0; i < DropTargetRules::count; ++i)
        if (p.targets.at(i)) {
            award(5);
            session_.reactor_remaining = sub(session_.reactor_remaining, 1);
            p.targets.at(i) = 0;
        }
    for (auto [timer, first] : {
             std::pair{F::target_bank_a_timer, 0u},
             std::pair{F::target_bank_b_timer, 3u}
    })
        if (!(rules_.*timer))
            for (unsigned i = 0; i < 3; ++i)
                if (DropTargetRules::state(session_, lamps_, first + i)) {
                    (rules_.*timer) = first ? bank_b_reset : bank_a_reset;
                    break;
                }
    session_.reactor_remaining = std::max(0, session_.reactor_remaining);
    if (session_.reactor_regeneration && rules_.switch_edge(reactor_collect_contact)) {
        rules_.mission_flash = 1;
        rules_.mission_flash_time = award_flash;
        session_.reactor_remaining = std::min(10, add(session_.reactor_remaining, 2));
    }
    countdown(); // The original tests completion even on the expiring frame.
    if (!session_.reactor_remaining) {
        session_.reactor_regeneration = 0;
        session_.mission_centre_lit = 0;
        if (rules_.switch_edge(reactor_collect_contact)) {
            award(50);
            animation(mission_animation::reactor_complete, replace_animation);
            complete();
            finish();
            page(MissionPage::total, result_ticks, -1);
            scoring_.sound({99, -64, 0, 1});
        }
    }
    progress(mul(sub(10, session_.reactor_remaining), 100) / 10);
}
void MissionRules::hunt() {
    if (effects_ && !effects_->mission_animation_active())
        animation(mission_animation::hunt_active);
    progress(mul(sub(5, session_.hunt_remaining), 100) / 5);
    if (session_.mission_centre_lit && rules_.switch_edge(centre_contact))
        session_.mission_centre_lit = 0;
    constexpr Switch contacts[] = {Switch::centre_sinkhole, Switch::upper_sinkhole, Switch::sinkhole_a, Switch::sinkhole_b};
    bool hit = false;
    if (session_.mission_centre_lit) {
        const auto target = unsigned(session_.hunt_target);
        hit = target < std::size(contacts) && rules_.switch_edge(contacts[target]);
    } else
        for (auto contact : contacts)
            hit |= rules_.switch_edge(contact);
    if (!hit) {
        countdown();
        return;
    }
    scoring_.sound({169, 0, 64, 18});
    award(10);
    if (!session_.mission_centre_lit) {
        LampRules{}.queue(rules_, session_, 68, award_flash);
        session_.mission_centre_lit = 1;
    }
    rules_.mission_flash = 1;
    rules_.mission_flash_time = award_flash;
    animation(mission_animation::hunt_hit, replace_animation | ((std::uint32_t(session_.hunt_remaining) & 1) ? mirror_animation : 0));
    animation(mission_animation::hunt_active);
    const auto before = session_.hunt_target;
    unsigned target;
    do {
        target = random_.next() & 3;
    } while (target == unsigned(before));
    session_.hunt_target = int(target);
    const auto remaining = sub(session_.hunt_remaining, 1);
    session_.hunt_remaining = remaining;
    if (!remaining) {
        award(50);
        animation(mission_animation::hunt_complete, replace_animation);
        complete();
        finish();
        page(MissionPage::total, result_ticks, -1);
        scoring_.sound({100, -64, 0, 1});
    }
}
void MissionRules::ammo_dump() {
    if (effects_ && !effects_->mission_animation_active())
        animation(mission_animation::ammo_stage_base, replace_animation | hold_animation);
    progress(mul(sub(25, session_.ammo_remaining), 100) / 25);
    rules_.return_toggle = 1;
    if (session_.mission_centre_lit && rules_.switch_edge(centre_contact)) {
        award(5);
        session_.mission_centre_lit = 0;
        session_.ammo_strength = 1;
    }
    if (rules_.switch_edge(Switch::bumper_a) || rules_.switch_edge(Switch::bumper_b) || rules_.switch_edge(Switch::bumper_c))
        session_.ammo_remaining = sub(session_.ammo_remaining, add(session_.ammo_strength, 1));
    const auto remaining = session_.ammo_remaining, stage = session_.ammo_stage;
    int threshold = stage == 0 ? 24 : (stage == 1 ? 15 : 7);
    if (stage <= 2 && remaining <= threshold) {
        constexpr int factors[] = {5, 15, 25};
        award(factors[std::max(0, stage)]);
        session_.ammo_stage = add(stage, 1);
        animation(mission_animation::ammo_stage_base + unsigned(std::max(0, stage)), replace_animation);
        animation(mission_animation::ammo_next_stage_base + unsigned(std::max(0, stage)), hold_animation);
        scoring_.sound({170, 0, 0, 2});
    }
    if (remaining < 1) {
        award(50);
        animation(mission_animation::ammo_complete, replace_animation);
        complete();
        finish();
        scoring_.sound({101, -64, 0, 1});
        scoring_.sound({170, 0, 0, 2});
        page(MissionPage::total, result_ticks, -1);
        return;
    }
    countdown();
}
void MissionRules::skyscraper() {
    const auto active = effects_ ? effects_->mission_animation_active() : 0;
    if (active == mission_animation::skyscraper_award || active == mission_animation::skyscraper_floor_award)
        page(MissionPage::skyscraper_award, 1, -1);
    if (effects_ && !active)
        animation(rules_.skyscraper_floor ? mission_animation::skyscraper_award : mission_animation::skyscraper_active);
    progress(mul(rules_.skyscraper_floor, 100) / 5);
    if (rules_.skyscraper_floor == 5 && !session_.skyscraper_extended)
        session_.mission_centre_lit = 1;
    if (session_.mission_centre_lit && rules_.switch_edge(centre_contact)) {
        session_.mission_centre_lit = 0;
        session_.skyscraper_extended = 1;
    }
    decrement(rules_, session_, S::skyscraper_cooldown);
    if ((rules_.switch_edge(Switch::spinner_a) || rules_.switch_edge(Switch::spinner_b)) && !session_.skyscraper_cooldown) {
        award(1);
        if (effects_)
            effects_->mission_clear_page();
        session_.skyscraper_cooldown = 4 * time_unit;
        const auto next = add(rules_.skyscraper_floor, 1), maximum = add(session_.skyscraper_extended, 5);
        rules_.skyscraper_floor = std::min(next, maximum);
        if (next <= maximum) {
            animation(mission_animation::skyscraper_climb, replace_animation);
            animation(mission_animation::skyscraper_floor_award);
            if (next >= 1 && next <= 6)
                scoring_.sound({110 + next, -64, 0, 1});
        }
    }
    const auto floor = rules_.skyscraper_floor;
    if (!floor || !rules_.switch_edge(sinkhole_contact)) {
        countdown();
        return;
    }
    finish();
    page(MissionPage::total, result_ticks, -1);
    constexpr int factors[] = {5, 10, 25, 50, 100, 100};
    if (floor >= 1 && floor <= int(std::size(factors))) {
        award(factors[floor - 1]);
        animation(mission_animation::skyscraper_collect, replace_animation);
        if (floor == 6 && extra_balls_)
            extra_balls_->award();
        if (floor >= 5)
            complete();
    }
}
void MissionRules::shuttle() {
    progress(mul(session_.shuttle_hits, 100) / 40);
    if (effects_ && !effects_->mission_animation_active())
        animation(mission_animation::shuttle_active);
    int contacts = 0;
    if (session_.shuttle_charge != 15)
        for (unsigned i = 0; i < 43; ++i)
            contacts += rules_.switch_edge(i);
    if (contacts) {
        const auto charge = add(session_.shuttle_charge, contacts);
        session_.shuttle_charge = charge > 15 ? 15 : charge;
    }
    if (rules_.switch_edge(Switch::ramp_a) || rules_.switch_edge(Switch::web_ramp)) {
        const auto charge = session_.shuttle_charge;
        animation(mission_animation::shuttle_ramp_hit, replace_animation, unsigned(charge));
        animation(mission_animation::shuttle_active);
        award(charge);
        session_.shuttle_charge = 3;
        session_.shuttle_hits = add(session_.shuttle_hits, charge);
        session_.mission_centre_lit = 1;
    }
    if (session_.mission_centre_lit && rules_.switch_edge(centre_contact)) {
        animation(mission_animation::shuttle_centre_hit, replace_animation);
        animation(mission_animation::shuttle_active);
        session_.mission_centre_lit = 0;
        session_.shuttle_hits = add(session_.shuttle_hits, 10);
        award(15);
    }
    if (session_.shuttle_hits > 39) {
        award(50);
        animation(mission_animation::shuttle_complete, replace_animation);
        complete();
        finish();
        scoring_.sound({127, -64, 0, 1});
        page(MissionPage::total, result_ticks, -1);
        return;
    }
    countdown();
}
void MissionRules::biker() {
    if (effects_ && !effects_->mission_animation_active())
        animation(mission_animation::biker_active);
    progress(mul(sub(4, state_.biker_remaining), 100) / 4);
    if (session_.mission_centre_lit && rules_.switch_edge(centre_contact)) {
        session_.mission_centre_lit = 0;
        state_.biker_timer = -1;
    }
    if (state_.biker_timer != -1) {
        state_.biker_timer = std::max(0, sub(state_.biker_timer, rules_.elapsed));
        if (!state_.biker_timer) {
            state_.biker_shot = add(state_.biker_shot, 1);
            if (state_.biker_shot == 4)
                state_.biker_shot = 0;
            state_.biker_timer = 6 * time_unit;
        }
    }
    bool hit = false;
    switch (state_.biker_shot) {
        case 0:
            hit = rules_.switch_edge(Switch::ramp_a);
            break;
        case 1:
            hit = rules_.switch_edge(Switch::web_ramp);
            break;
        case 2:
            hit = rules_.left_loop_completed != 0;
            break;
        case 3:
            hit = session_.right_loop_completed != 0;
            break;
    }
    if (hit) {
        session_.mission_centre_lit = 1;
        state_.biker_shot = add(state_.biker_shot, 1);
        if (state_.biker_shot == 4)
            state_.biker_shot = 0;
        state_.biker_alternate = 1;
    }
    if (rules_.loop_completed && state_.biker_alternate) {
        hit = true;
        state_.biker_alternate = 0;
    }
    if (!hit) {
        countdown();
        return;
    }
    scoring_.sound({169, 0, 64, 18});
    animation(rules_.frame_phase() & 0x10 ? mission_animation::biker_hit_a : mission_animation::biker_hit_b, replace_animation);
    animation(mission_animation::biker_active);
    award(state_.biker_award);
    rules_.mission_flash = 1;
    rules_.mission_flash_time = award_flash;
    state_.biker_award = add(state_.biker_award, 5);
    state_.biker_timer = 6 * time_unit;
    state_.biker_remaining = sub(state_.biker_remaining, 1);
    if (!state_.biker_remaining) {
        award(50);
        animation(mission_animation::biker_complete, replace_animation);
        complete();
        finish();
        page(MissionPage::total, result_ticks, -1);
        scoring_.sound({44, 64, 0, 1});
    }
}
void MissionRules::step() {
    if (!rules_.mission_time)
        return;
    switch (Mission(session_.mission_kind)) {
        case Mission::reactor:
            reactor();
            break;
        case Mission::hunt:
            hunt();
            break;
        case Mission::ammo_dump:
            ammo_dump();
            break;
        case Mission::skyscraper:
            skyscraper();
            break;
        case Mission::shuttle:
            shuttle();
            break;
        case Mission::biker:
            biker();
            break;
        default:
            throw std::runtime_error("Invalid active mission");
    }
}
MissionStatus MissionRules::status() const {
    MissionStatus result;
    const auto active = rules_.mission_time != 0;
    const auto kind = active ? unsigned(session_.mission_kind) : unsigned(session_.players().at(std::uint32_t(rules_.current_player)).missions.selected);
    result.name = name(Mission(kind));
    result.active = active;
    result.seconds = unsigned(std::max(0, rules_.mission_time / time_unit - 2));
    if (!active) {
        result.objective = rules_.drain_extension ? "Centre sinkhole starts mission" : "Shoot Web ramp to light mission";
        return result;
    }
    switch (Mission(kind)) {
        case Mission::reactor:
            result.target = 10;
            result.progress = unsigned(std::clamp(10 - session_.reactor_remaining, 0, 10));
            result.objective = result.progress == 10 ? "Shoot upper hole to complete" : "Shoot drop targets";
            if (session_.reactor_regeneration)
                result.objective = "Centre shot stops regeneration; hit drops";
            break;
        case Mission::hunt: {
            constexpr const char* targets[] = {"Centre sinkhole", "Upper hole", "Left sinkhole", "Right sinkhole"};
            result.target = 5;
            result.progress = unsigned(std::clamp(5 - session_.hunt_remaining, 0, 5));
            const auto target = unsigned(session_.hunt_target);
            result.objective = target < std::size(targets) ? targets[target] : "Shoot a sinkhole";
            if (!session_.mission_centre_lit)
                result.objective = "Shoot any sinkhole";
            break;
        }
        case Mission::ammo_dump:
            result.target = 25;
            result.progress = unsigned(std::clamp(25 - session_.ammo_remaining, 0, 25));
            result.objective = "Shoot jet bumpers";
            break;
        case Mission::skyscraper:
            result.target = 5;
            result.progress = unsigned(std::max(0, rules_.skyscraper_floor));
            result.objective = "Spinners climb; centre sinkhole collects";
            break;
        case Mission::shuttle:
            result.target = 40;
            result.progress = unsigned(std::clamp(session_.shuttle_hits, 0, 40));
            result.objective = "Shoot ramps; centre shot adds 10";
            break;
        case Mission::biker: {
            constexpr const char* shots[] = {"Shoot left ramp", "Shoot Web ramp", "Shoot left loop", "Shoot right loop"};
            result.target = 4;
            result.progress = unsigned(std::clamp(4 - state_.biker_remaining, 0, 4));
            const auto shot = unsigned(state_.biker_shot);
            result.objective = shot < std::size(shots) ? shots[shot] : "Shoot lit ramp or loop";
            break;
        }
        default:
            break;
    }
    return result;
}
void MissionRules::compose_lamps() {
    LampRules lights;
    const auto blink = [&](unsigned lamp, int duration = fast_blink) { lights.blink(rules_, session_, lamps_, lamp, duration); };
    const auto out = [&](unsigned lamp, int value = 1) { LampRules::value(session_, lamps_, lamp, value); };
    const auto pattern = [&](unsigned offset, unsigned mask) { return int(((rules_.frame_phase() + offset) & mask) != 0); };
    switch (Mission(session_.mission_kind)) {
        case Mission::reactor:
            if (!session_.reactor_remaining) {
                constexpr unsigned groups[][3] = {
                    {58, 59, 52},
                    {57, 60, 51},
                    {56, 61, 50}
                };
                for (unsigned i = 0; i < std::size(groups); ++i)
                    if (pattern(i * 4, 16))
                        for (auto lamp : groups[i])
                            out(lamp);
            } else if (!rules_.mission_flash) {
                for (unsigned lamp = 56; lamp <= 61; ++lamp)
                    blink(lamp);
                if (session_.reactor_regeneration)
                    blink(50, slow_blink);
            } else if (rules_.mission_flash == 1)
                blink(50, 3 * time_unit);
            break;
        case Mission::hunt: {
            constexpr unsigned lamps[] = {55, 52, 38, 30};
            if (!session_.mission_centre_lit)
                for (auto lamp : lamps)
                    blink(lamp);
            else if (const auto target = unsigned(session_.hunt_target); target < std::size(lamps))
                blink(lamps[target], slow_blink);
            if (rules_.mission_flash == 1)
                for (auto lamp : lamps)
                    blink(lamp, slow_blink);
            break;
        }
        case Mission::ammo_dump:
            if (!(rules_.frame_phase() & 7))
                state_.bumper_lamp_phase = add(state_.bumper_lamp_phase, 1);
            if (state_.bumper_lamp_phase > 2)
                state_.bumper_lamp_phase = 0;
            for (unsigned i = 0; i < 3; ++i)
                out(70 + i, state_.bumper_lamp_phase == int(i));
            break;
        case Mission::skyscraper: {
            const auto floor = rules_.skyscraper_floor;
            if (floor == 6 || (floor == 5 && !session_.skyscraper_extended)) {
                out(54, pattern(4, 32));
                out(34, pattern(8, 32));
                out(53, pattern(12, 32));
                out(55, pattern(0, 32));
            } else {
                if (!session_.skyscraper_cooldown) {
                    constexpr unsigned lanes[][2] = {
                        {35, 32},
                        {33, 31},
                        {27, 23},
                        {26, 22},
                        {25, 21},
                        {24, 20}
                    };
                    for (unsigned i = 0; i < std::size(lanes); ++i)
                        for (auto lamp : lanes[i])
                            out(lamp, pattern((i + 1) * 4, 32));
                }
                if (floor)
                    blink(55);
            }
            break;
        }
        case Mission::shuttle: {
            const auto duration = session_.shuttle_charge == 15 ? fast_blink : slow_blink;
            blink(47, duration);
            blink(44, duration);
            break;
        }
        case Mission::biker: {
            constexpr unsigned shots[] = {47, 44, 35, 32};
            if (const auto shot = unsigned(state_.biker_shot); shot < std::size(shots))
                blink(shots[shot], state_.biker_timer == -1 ? fast_blink : state_.biker_timer);
            if (state_.biker_alternate)
                blink(46);
            if (rules_.mission_flash) {
                for (auto lamp : shots)
                    blink(lamp, slow_blink);
                blink(46, slow_blink);
            }
            break;
        }
        default:
            throw std::runtime_error("Invalid mission lamp dispatcher");
    }
}
} // namespace web::rules
