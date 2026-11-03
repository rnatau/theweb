#include "ball_rules.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace web::rules {
using namespace physics;
using namespace physics::integer;
namespace F = web::rules::turn_field;
namespace {
void sound(SimulationState& simulation, TurnState& rules, std::size_t slot, BallRuleEffectSink& sink, int id, int pan, int group) {
    sink.on_effect(
        web::rules::BallRuleEffect{
            slot,
            web::SoundRequest{id, pan, 0, group}
    },
        simulation,
        rules);
}
void display(SimulationState& simulation, TurnState& rules, std::size_t slot, BallRuleEffectSink& sink, int id, int flags) {
    sink.on_effect(
        web::rules::BallRuleEffect{
            slot,
            web::AnimationRequest{id, flags}
    },
        simulation,
        rules);
}
} // namespace
void TurnState::reset() {
    mission_time = 0;
    kickback_rearm_timer = 0;
    loop_remaining = 0;
    station_centre_stage = 0;
    left_loop_completed = 0;
    reserved_0 = 0;
    station_stage_code = 0;
    magnet_voice_timer = 0;
    station_ramp_ready = 0;
    left_loop_completion_timer = 0;
    right_spinner_timer = 0;
    magnet_combo_timer = 0;
    inlane_a_timer = 0;
    magnet_activation_window = 0;
    left_spinner_lane_timer = 0;
    inlane_b_timer = 0;
    station_triple_ready = 0;
    reserved_1 = 0;
    right_spinner_lane_timer = 0;
    web_ramp_entry_timer = 0;
    mission_started_this_frame = 0;
    announce_pending = 0;
    save_timer = 0;
    loop_completed = 0;
    auto_launch_block = 0;
    auto_kick_enabled = 0;
    quick_shot_value = 0;
    quick_shot_timer = 0;
    player_cycle = 0;
    serve_spacing_timer = 0;
    return_toggle = 0;
    loop_first_gate_timer = 0;
    super_rescue = 0;
    quick_shot_warning = 0;
    serve_requests = 0;
    diverter_enabled = 0;
    diverter_warning = 0;
    extra_ball_turn = 0;
    loop_last_gate_timer = 0;
    extra_ball_diverter = 0;
    super_jackpot = 0;
    quick_shot_side = 0;
    station_super_lit = 0;
    ramp_a_entry_timer = 0;
    showdown_hits = 0;
    positive_sling_inhibit = 0;
    negative_sling_inhibit = 0;
    lamp_queue = 0;
    centre_award_consumed = 0;
    announcement_voice = 0;
    target_bank_b_timer = 0;
    outlane_extra_ball = 0;
    mission_flash = 0;
    super_launcher_timer = 0;
    target_bank_a_timer = 0;
    super_launcher_hits = 0;
    super_launcher_countdown = 0;
    showdown_phase = 0;
    showdown_stage = 0;
    mission_flash_time = 0;
    mission_intro = 0;
    explosion_wait = 0;
    showdown_rotating_lamp = 0;
    drain_extension = 0;
    combined_lock = 0;
    phase_code = 0;
    warnings_left = 0;
    status_right_hold = 0;
    centre_award_wait = 0;
    lock_choice_timer = 0;
    status_selection = 0;
    tilt_lock = 0;
    status_timer = 0;
    launch_timer = 0;
    reserved_2 = 0;
    continue_timer = 0;
    elapsed = 0;
    tilted = 0;
    single_ball_award_mode = 0;
    reserved_3 = 0;
    bonus_stage = 0;
    reserved_4 = 0;
    lamp_frame = 0;
    status_left_hold = 0;
    reserved_5 = 0;
    lamp_mode = 0;
    record_scan_index = 0;
    showdown_artwork = 0;
    record_best_high = 0;
    record_rank = 0;
    record_best_low = 0;
    showdown_sound_timer = 0;
    record_best_player = 0;
    showdown_finish_timer = 0;
    initials_confirmation_timer = 0;
    match_timer = 0;
    record_maximum = 0;
    showdown_acceleration = 0;
    kickback_announcement_played = 0;
    lamp_animation = 0;
    reserved_6 = 0;
    showdown_sound_phase = 0;
    flipper_timing_window = 0;
    music_fade_timer = 0;
    award_retrigger_guard = 0;
    bonus_display_gate = 0;
    music_requested_track = 0;
    showdown_intro_timer = 0;
    match_awarded = 0;
    current_player = 0;
    lamp_previous_frame_phase = 0;
    start_latch = 0;
    player_turn = 0;
    video_active = 0;
    player_count = 0;
    launch_enabled = 0;
    returns_paused = 0;
    mission_total = 0;
    super_jet_total = 0;
    showdown_total = 0;
    startup_display_ticks = 0;
    video_lock_pending = 0;
    lock_choice = 0;
    showdown_reserves = 0;
    skyscraper_floor = 0;
    showdown_intro = 0;
    match_digit = 0;
    drain_override = 0;
    match_stage = 0;
    super_skillshot = 0;
    initials_cursor = 0;
    bonus_timer = 0;
    record_display_rank = 0;
    record_entry_player = 0;
    live_balls = 0;
    previous_switches_.fill(0);
    switch_edges_.fill(0);
    switch_contacts_.fill(0);
    frame_phase_ = 0;
    for (auto& player : players_)
        player.ball.relaunch = 0;
}
std::int32_t& TurnState::previous_switch(std::size_t index) {
    switch (index) {
        case 10:
            return positive_sling_inhibit;
        case 11:
            return negative_sling_inhibit;
        default:
            return previous_switches_.at(index);
    }
}
const std::int32_t& TurnState::previous_switch(std::size_t index) const {
    switch (index) {
        case 10:
            return positive_sling_inhibit;
        case 11:
            return negative_sling_inhibit;
        default:
            return previous_switches_.at(index);
    }
}
std::int32_t& TurnState::edge_switch(std::size_t index) {
    switch (index) {
        default:
            return switch_edges_.at(index);
    }
}
const std::int32_t& TurnState::edge_switch(std::size_t index) const {
    switch (index) {
        default:
            return switch_edges_.at(index);
    }
}
std::int32_t& TurnState::contact_switch(std::size_t index) {
    switch (index) {
        case 43:
            return target_bank_b_timer;
        case 44:
            return outlane_extra_ball;
        case 45:
            return mission_flash;
        case 46:
            return super_launcher_timer;
        case 47:
            return target_bank_a_timer;
        case 48:
            return super_launcher_hits;
        case 49:
            return super_launcher_countdown;
        case 50:
            return showdown_phase;
        case 51:
            return showdown_stage;
        case 52:
            return mission_flash_time;
        case 53:
            return mission_intro;
        case 54:
            return explosion_wait;
        case 55:
            return showdown_rotating_lamp;
        case 56:
            return drain_extension;
        case 57:
            return combined_lock;
        case 58:
            return phase_code;
        case 59:
            return warnings_left;
        case 60:
            return status_right_hold;
        case 61:
            return centre_award_wait;
        case 62:
            return lock_choice_timer;
        default:
            return switch_contacts_.at(index);
    }
}
const std::int32_t& TurnState::contact_switch(std::size_t index) const {
    switch (index) {
        case 43:
            return target_bank_b_timer;
        case 44:
            return outlane_extra_ball;
        case 45:
            return mission_flash;
        case 46:
            return super_launcher_timer;
        case 47:
            return target_bank_a_timer;
        case 48:
            return super_launcher_hits;
        case 49:
            return super_launcher_countdown;
        case 50:
            return showdown_phase;
        case 51:
            return showdown_stage;
        case 52:
            return mission_flash_time;
        case 53:
            return mission_intro;
        case 54:
            return explosion_wait;
        case 55:
            return showdown_rotating_lamp;
        case 56:
            return drain_extension;
        case 57:
            return combined_lock;
        case 58:
            return phase_code;
        case 59:
            return warnings_left;
        case 60:
            return status_right_hold;
        case 61:
            return centre_award_wait;
        case 62:
            return lock_choice_timer;
        default:
            return switch_contacts_.at(index);
    }
}
std::int32_t TurnState::switch_word(std::size_t index) const {
    return contact_switch(index);
}
bool TurnState::switch_edge(std::size_t index) const {
    return edge_switch(index) != 0;
}
void TurnState::clear_switch_edge(TableSwitch contact) {
    edge_switch(std::size_t(contact)) = 0;
}
void TurnState::sample_switches() {
    for (unsigned index = 0; index < 43; ++index) {
        const auto current = contact_switch(index), previous = previous_switch(index);
        previous_switch(index) = current;
        contact_switch(index) = 0;
        edge_switch(index) = (previous ^ 1) & current;
    }
}
void TurnState::switch_contact(std::int32_t index) {
    index = std::max(index, 0);
    if (index < 64) {
        contact_switch(unsigned(index)) = 1;
        return;
    }
    switch (index) {
        case 64:
            contact_switch(29) = contact_switch(30) = 1;
            break;
        case 65:
            contact_switch(28) = contact_switch(29) = 1;
            break;
        case 66:
            contact_switch(25) = contact_switch(26) = 1;
            break;
        case 67:
            contact_switch(26) = contact_switch(27) = 1;
            break;
        default:
            break;
    }
}
void BallLifecycle::serve(SimulationState& simulation, TurnState& rules, BallRuleEffectSink& effects) const {
    std::size_t selected = simulation.balls.size();
    for (std::size_t slot = 0; slot < simulation.balls.size(); slot++)
        if (simulation.balls[slot].active() == 0)
            selected = slot;
    if (selected == simulation.balls.size())
        return;
    sound(simulation, rules, selected, effects, 160, 64, 15);
    auto& ball = simulation.balls[selected];
    ball.place_in_launcher();
}
void BallLifecycle::auto_launch(SimulationState& simulation, TurnState& rules, std::size_t slot, BallRuleEffectSink& effects) const {
    auto& ball = simulation.balls.at(slot);
    sound(simulation, rules, slot, effects, 158, 64, 99);
    auto velocity = ball.velocity();
    velocity.y = std::int32_t(simulation.random.next() & 0xfff) + 50000;
    ball.velocity(velocity);
    ball.motion_type(physics::ball_type::free);
}
void BallLifecycle::begin_launch_window(TurnState& rules) const {
    if (rules.launch_enabled == 1) {
        rules.launch_timer = 0x24000;
        rules.save_timer = 0x78000;
    } else
        rules.save_timer = 0x50000;
    rules.launch_enabled = 0;
}
void BallLifecycle::launch(SimulationState& simulation, TurnState& rules, BallRuleEffectSink& effects) const {
    if (rules.launch_enabled == 0)
        return;
    if (selection_) {
        if (const auto slot = selection_(simulation.balls)) {
            auto_launch(simulation, rules, *slot, effects);
            begin_launch_window(rules);
        }
        return;
    }
    for (std::size_t slot = 0; slot < simulation.balls.size(); slot++) {
        auto& ball = simulation.balls[slot];
        if (ball.transition_kind() == 16 && ball.motion_type() == physics::ball_type::launcher) {
            auto_launch(simulation, rules, slot, effects);
            begin_launch_window(rules);
        }
    }
}
void BallLifecycle::eject(SimulationState& simulation, TurnState& rules, std::size_t slot, Exit exit, BallRuleEffectSink& effects) const {
    auto& ball = simulation.balls.at(slot);
    sound(simulation, rules, slot, effects, exit == Exit::right ? 159 : 165, exit == Exit::left ? -64 : (exit == Exit::right ? 64 : 32), exit == Exit::lift ? 99 : 13);
    if (exit == Exit::lift) {
        ball.begin_lift();
        return;
    }
    ball.return_to_playfield(exit == Exit::right ? Position2{0x10d0000, 0x1390000} : Position2{0xf0000, 0x16e0000}, exit == Exit::right ? Vector2{-1000, -9000} : Vector2{0, -10000});
}
void BallLifecycle::drained(SimulationState& simulation, TurnState& rules, std::size_t slot) const {
    auto& ball = simulation.balls.at(slot);
    switch (ball.transition_kind()) {
        case 0:
            rules.switch_contact(20);
            ball.motion_type(physics::ball_type::centre_return);
            ball.rule_timer(rules.mission_time == 0 && rules.single_ball_award_mode != 2 && rules.drain_override == 0 && rules.drain_extension != 0 ? 0x10000 : 0x6000);
            break;
        case 1:
            ball.motion_type(physics::ball_type::upper_return);
            rules.switch_contact(31);
            ball.rule_timer(0x6000);
            break;
        case 2:
            ball.motion_type(physics::ball_type::lift_return);
            rules.switch_contact(8);
            ball.rule_timer(0x6000);
            break;
        case 3:
            rules.switch_contact(7);
            ball.rule_timer(0x6000);
            ball.motion_type(physics::ball_type::left_return);
            break;
        default:
            break;
    }
}
void BallLifecycle::update_returns(SimulationState& simulation, TurnState& rules, BallRuleEffectSink& effects) const {
    for (std::size_t slot = 0; slot < simulation.balls.size(); slot++) {
        auto& ball = simulation.balls[slot];
        if (ball.active() == 0 || rules.returns_paused != 0)
            continue;
        ball.rule_timer(std::max(0, sub(ball.rule_timer(), rules.elapsed)));
        if (ball.rule_timer() != 0)
            continue;
        switch (ball.motion_type()) {
            case physics::ball_type::centre_return:
                eject(simulation, rules, slot, rules.return_toggle == 0 ? Exit::left : Exit::right, effects);
                rules.return_toggle = add(rules.return_toggle, 1) & 1;
                break;
            case physics::ball_type::upper_return:
                eject(simulation, rules, slot, Exit::right, effects);
                rules.return_toggle = 0;
                break;
            case physics::ball_type::lift_return:
                eject(simulation, rules, slot, Exit::lift, effects);
                if ((simulation.actions & input_action::both_flippers) == input_action::both_flippers)
                    display(simulation, rules, slot, effects, 69, 3);
                break;
            case physics::ball_type::left_return:
                eject(simulation, rules, slot, Exit::left, effects);
                rules.return_toggle = 1;
                break;
            case physics::ball_type::pending_drain:
                ball.active(0);
                ball.motion_mode(physics::motion_mode::table);
                if (rules.save_timer != 0) {
                    rules.serve_requests = add(rules.serve_requests, 1);
                    if (rules.drain_override == 0) {
                        display(simulation, rules, slot, effects, 32, 1);
                        sound(simulation, rules, slot, effects, (rules.frame_phase() & 0x10) ? 128 : 43, (rules.frame_phase() & 0x10) ? -64 : 64, 1);
                    }
                } else
                    rules.live_balls = sub(rules.live_balls, 1);
                break;
            case physics::ball_type::station_capture:
                ball.active(0);
                ball.motion_mode(physics::motion_mode::table);
                ball.motion_type(physics::ball_type::none);
                if (rules.live_balls == 1)
                    rules.serve_requests = add(rules.serve_requests, 1);
                else {
                    simulation.balls[0].active(1);
                    eject(simulation, rules, 0, Exit::left, effects);
                    simulation.balls[1].active(1);
                    eject(simulation, rules, 1, Exit::right, effects);
                    rules.serve_requests = add(rules.serve_requests, 1);
                    const auto player = rules.current_player;
                    if (player < 0 || player >= 4)
                        throw std::out_of_range("Unrecovered current player slot");
                    if (rules.player_relaunch(std::size_t(player)) != 0) {
                        rules.player_relaunch(std::size_t(player), 0);
                        rules.serve_requests = add(rules.serve_requests, 1);
                    }
                }
                break;
            default:
                break;
        }
    }
}
void BallRuleCallbacks::on_event(const PhysicsEvent& event, BallSlots& balls) {
    if (&balls != &simulation_.balls)
        throw std::invalid_argument("Rule callbacks require their owning ball slots");
    if (event.kind() == PhysicsEvent::Kind::drain)
        lifecycle_.drained(simulation_, rules_, event.slot);
    else if (event.kind() == PhysicsEvent::Kind::switch_contact)
        rules_.switch_contact(event.get<SwitchContact>().switch_id);
    else if (event.kind() == PhysicsEvent::Kind::surface_rule)
        lifecycle_.surface_transition(simulation_, rules_, event.slot, event.get<SurfaceContact>().transition, effects_);
    else
        remaining_.on_event(event, balls);
}
void BallLifecycle::surface_transition(SimulationState& simulation, TurnState& rules, std::size_t slot, std::int32_t transition, BallRuleEffectSink& effects) const {
    auto& ball = simulation.balls.at(slot);
    ball.transition_kind(transition);
    if (transition >= 0 && transition <= 3) {
        sound(simulation, rules, slot, effects, 161, transition == 0 ? 0 : (transition == 3 ? -64 : 32), 99);
        ball.motion_mode(physics::motion_mode::draining);
        const auto velocity = ball.velocity();
        const auto absolute = [](std::int32_t value) { return value < 0 ? sub(0, value) : value; };
        ball.vertical_velocity(sub(0, add(absolute(velocity.x / 2), absolute(velocity.y / 2))));
    } else if (transition == 4 || transition == 5) {
        rules.switch_contact(transition == 4 ? 12 : 9);
        SpinnerDynamics{}.kick(simulation.mechanisms, std::size_t(transition - 4), ball.velocity().y / 8);
    } else if (transition == 6 || transition == 7) {
        const bool positive = transition == 6;
        if ((positive ? ball.velocity().x <= 2048 : ball.velocity().x >= -2048) || rules.tilt_lock != 0)
            return;
        if ((positive ? rules.positive_sling_inhibit : rules.negative_sling_inhibit) == 0) {
            // Original x87 double intermediates, followed by explicit single
            // stores for the two kick limits. Constants are original float bits.
            const auto position = ball.position();
            const auto y = position.y >> 16;
            const double distance = position.y <= 0xbe0000 ? double(y) - 160.0 : 220.0 - double(y);
            const double random_x = double((simulation.random.next() & 0x1fff) + 28000);
            const float minimum_x = std::max(0.0f, float((distance * random_x) * double(std::bit_cast<float>(std::uint32_t(0x3d088889)))));
            const double random_y = double((simulation.random.next() & 0x1fff) + 28000);
            const float minimum_y = std::max(0.0f, float((distance * random_y) * double(std::bit_cast<float>(std::uint32_t(0x3c088889)))));
            auto velocity = ball.velocity();
            const double x = positive ? double(minimum_x) : -double(minimum_x);
            if (positive ? double(velocity.x) < x : double(velocity.x) > x)
                velocity.x = std::int32_t(x);
            if (velocity.y < 0)
                velocity.y = std::int32_t(double(velocity.y) + double(minimum_y));
            else if (double(velocity.y) < double(minimum_y))
                velocity.y = std::int32_t(minimum_y);
            ball.velocity({std::clamp(velocity.x, -48000, 48000), std::clamp(velocity.y, -48000, 48000)});
            // Both original sling arms pass the same sample and pan arguments.
            sound(simulation, rules, slot, effects, 162, -64, 6);
        }
        rules.switch_contact(positive ? 10 : 11);
    } else if (transition >= 8 && transition <= 10) {
        const auto velocity = ball.velocity();
        if ((velocity.x >= -2048 && velocity.x <= 2048 && velocity.y >= -2048 && velocity.y <= 2048) || (simulation.actions & input_action::both_flippers) == input_action::both_flippers)
            return;
        if (rules.tilt_lock == 0) {
            static constexpr std::array<Vector2, 3> centers = {
                Vector2{0x450000, 0x2090000},
                Vector2{0x6e0000, 0x1e30000},
                Vector2{0x7d0000, 0x21b0000}
            };
            const auto center = centers[std::size_t(transition - 8)];
            const auto dx = sub(ball.position().x, center.x);
            const auto divisor_x = std::int32_t((simulation.random.next() & 0x7f) + 64);
            const auto dy = sub(ball.position().y, center.y);
            const auto divisor_y = std::int32_t((simulation.random.next() & 0x7f) + 64);
            ball.velocity({std::clamp(dx / divisor_x, -48000, 48000), std::clamp(dy / divisor_y, -48000, 48000)});
            sound(simulation, rules, slot, effects, transition == 8 ? 156 : (transition == 9 ? 157 : 164), 0, transition);
        }
        rules.switch_contact(transition + 24);
    } else if (transition == 11) {
        effects.on_effect(web::rules::BallRuleEffect{slot, web::rules::StationContact{}}, simulation, rules);
    } else if (transition == 12) {
        auto velocity = ball.velocity();
        if (rules.diverter_enabled != 0 && velocity.x < 0) {
            velocity.y = add(velocity.y, velocity.x);
            velocity.x = 0;
            ball.velocity(velocity);
        }
    } else if (transition == 15) {
        if (rules.auto_kick_enabled != 0 && rules.tilt_lock == 0 && ball.velocity().y < 0x8000) {
            auto velocity = ball.velocity();
            velocity.y = std::int32_t((simulation.random.next() & 0x3fff) + 0x8000);
            ball.velocity(velocity);
            sound(simulation, rules, slot, effects, 158, -64, 99);
        }
        rules.switch_contact(1);
    } else if (transition == 16) {
        rules.switch_contact(38);
        if (rules.launch_enabled == 0 && rules.auto_launch_block == 0)
            auto_launch(simulation, rules, slot, effects);
    } else if (transition == 17) {
        sound(simulation, rules, slot, effects, 150, 0, 99);
        if (rules.drain_override == 0 && rules.save_timer < 0x8000)
            effects.on_effect(
                web::rules::BallRuleEffect{
                    slot,
                    web::SoundRequest{182, 0, 64, 18}
            },
                simulation,
                rules);
        ball.await_drain_removal(rule_time::second);
    }
}
} // namespace web::rules
