#include "shot_sequences.hpp"
#include "lamps.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
using Switch = TableSwitch;
namespace {
constexpr int sequence_window = 2 * rule_time::second, spinner_window = rule_time::second, lane_window = 4 * rule_time::second, lane_flash = rule_time::half_second;
} // namespace
void ShotSequenceRules::step() {
    const auto edge = [&](Switch contact) { return rules_.switch_edge(contact); };
    auto& right_lane = rules_.right_spinner_lane_timer;
    auto& left_lane = rules_.left_spinner_lane_timer;
    auto& right_spinner = rules_.right_spinner_timer;
    auto& left_completion = rules_.left_loop_completion_timer;
    auto& left_completed = rules_.left_loop_completed;
    auto& alternate = rules_.loop_completed;
    auto& left_spinner = session_.left_spinner_timer;
    auto& right_completion = session_.right_loop_completion_timer;
    auto& right_completed = session_.right_loop_completed;
    auto& right_loop_lamp = session_.right_loop_lamp_timer;
    auto& left_loop_lamp = session_.left_loop_lamp_timer;
    const auto timer = [&](std::int32_t& value) {
        if (value)
            value = std::max(0, physics::integer::sub(value, rules_.elapsed));
    };
    const bool lanes_available = !rules_.drain_override && !rules_.quick_shot_timer;
    if (edge(Switch::spinner_a_lane_entry) && lanes_available) {
        LampRules{}.queue(rules_, session_, 18, lane_flash);
        right_lane = lane_window;
    }
    if (right_lane) {
        timer(right_lane);
        if (edge(Switch::spinner_a)) {
            right_lane = 0;
            if (power_)
                power_->advance();
        }
    }
    if (edge(Switch::spinner_a)) {
        right_spinner = spinner_window;
        scoring_.spinner();
    }
    if (right_spinner) {
        timer(right_spinner);
        if (edge(Switch::loop_gate_b)) {
            right_spinner = 0;
            if (!alternate)
                right_completion = sequence_window;
        }
    }
    right_completed = 0;
    if (right_completion) {
        timer(right_completion);
        if (edge(Switch::loop_gate_a)) {
            right_completion = 0;
            if (!rules_.drain_override)
                right_loop_lamp = lane_window;
            right_completed = 1;
        }
    }
    if (edge(Switch::spinner_b_lane_entry) && lanes_available) {
        LampRules{}.queue(rules_, session_, 16, lane_flash);
        left_lane = lane_window;
    }
    if (left_lane) {
        timer(left_lane);
        if (edge(Switch::spinner_b)) {
            left_lane = 0;
            if (power_)
                power_->advance();
        }
    }
    if (edge(Switch::spinner_b)) {
        left_spinner = spinner_window;
        scoring_.spinner(1);
    }
    if (left_spinner) {
        timer(left_spinner);
        if (edge(Switch::loop_gate_a)) {
            left_spinner = 0;
            left_completion = sequence_window;
        }
    }
    left_completed = 0;
    if (left_completion) {
        timer(left_completion);
        if (edge(Switch::loop_gate_b)) {
            left_completion = 0;
            if (!rules_.drain_override)
                left_loop_lamp = lane_window;
            left_completed = 1;
        }
    }
    if (lanes_available) {
        if (left_completed)
            left_lane = lane_window;
        if (right_completed)
            right_lane = lane_window;
    }
}
} // namespace web::rules
