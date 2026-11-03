#pragma once
#include "../input.hpp"
#include <initializer_list>
#include <cstdint>

namespace web {
// MENU 0x407ecd..0x4080d9. The slideshow's event arm reverses the
// vertical arrows and treats Space as forward. Host flipper aliases are added
// separately; these fixed Windows virtual keys come from the original code.
constexpr std::uint32_t frontend_key_action(unsigned key, bool slideshow = false) {
    if (key == key_code::enter || key == key_code::select)
        return input_action::launch;
    if (key == key_code::escape)
        return input_action::escape;
    if (slideshow) {
        for (const auto candidate : {key_code::left_shift, key_code::left_control, key_code::numpad_4, key_code::left, key_code::numpad_2, key_code::down})
            if (key == candidate)
                return input_action::left_flipper;
        for (const auto candidate : {key_code::right_shift, key_code::right_control, key_code::numpad_6, key_code::right, key_code::numpad_8, key_code::space, key_code::up})
            if (key == candidate)
                return input_action::right_flipper;
    } else {
        for (const auto candidate : {key_code::up, key_code::left_shift, key_code::left_control, key_code::numpad_4, key_code::left, key_code::numpad_8})
            if (key == candidate)
                return input_action::left_flipper;
        for (const auto candidate : {key_code::down, key_code::right, key_code::numpad_6, key_code::right_shift, key_code::right_control, key_code::numpad_2})
            if (key == candidate)
                return input_action::right_flipper;
    }
    return 0;
}
} // namespace web
