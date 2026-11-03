#include "../editions/enhanced/nudge_animation.hpp"
#include <iostream>
#include <stdexcept>
namespace {
unsigned checks = 0;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
    ++checks;
}
} // namespace
int main() {
    try {
        using namespace web;
        NudgeAnimation nudge;
        check(nudge.offset(0x70) == NudgeOffset{}, "Resting nudge displaced the table");
        for (const auto mode : {0x10u, 0x20u, 0x30u, 0x50u, 0x60u, 0x70u}) {
            const int distance = 2 * (1 + ((mode & 0x30) >> 4));
            for (const auto action : {input_action::nudge_negative, input_action::nudge_positive, input_action::nudge_up}) {
                nudge.trigger(action);
                const auto peak = nudge.offset(mode);
                check(
                    peak == (action == input_action::nudge_negative   ? NudgeOffset{distance, 0}
                             : action == input_action::nudge_positive ? NudgeOffset{-distance, 0}
                                                                      : NudgeOffset{0, -distance}),
                    "Nudge moved in the wrong direction or used the wrong resolution scale");
                auto previous = peak;
                for (unsigned tick = 0; tick < 12; ++tick) {
                    nudge.advance(1);
                    const auto next = nudge.offset(mode);
                    check(std::abs(next.x) <= std::abs(previous.x) && std::abs(next.y) <= std::abs(previous.y), "Nudge did not settle monotonically");
                    previous = next;
                }
                check(nudge.offset(mode) == NudgeOffset{}, "Nudge did not return exactly to its original position");
            }
        }
        nudge.trigger(input_action::nudge_negative | input_action::nudge_positive);
        check(nudge.offset(0x70) == NudgeOffset{0, -8}, "Opposing horizontal nudges must give the physics controller's upward action");
        nudge.trigger(input_action::nudge_up | input_action::nudge_negative);
        check(nudge.offset(0x70) == NudgeOffset{8, -8}, "Combined nudge lost an axis");
        nudge.trigger(input_action::nudge_positive);
        check(nudge.offset(0x70) == NudgeOffset{-8, 0}, "Repeated nudges accumulated displacement or retained a stale axis");
        nudge.advance(100);
        check(nudge.offset(0x70) == NudgeOffset{}, "Catch-up steps underflowed the nudge animation");
        std::cout << checks << " Enhanced nudge animation checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
