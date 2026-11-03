#pragma once
#include <cstdint>

namespace web::input_action {
// Original game/MENU action bits, independent of physical key bindings. Keep
// masks as integers so recorded input streams retain their original encoding.
inline constexpr std::uint32_t left_flipper = 1, right_flipper = 2;
inline constexpr std::uint32_t launch = 4, start = 8, continue_game = 0x10;
inline constexpr std::uint32_t nudge_up = 0x20, ball_control = 0x40, escape = 0x80;
inline constexpr std::uint32_t nudge_positive = 0x100, nudge_negative = 0x200;
inline constexpr auto both_flippers = left_flipper | right_flipper;
inline constexpr auto confirm = launch | start;
inline constexpr auto menu_navigation = both_flippers | launch;
inline constexpr auto any_nudge = nudge_up | nudge_positive | nudge_negative;
} // namespace web::input_action

namespace web::key_code {
// wgame.cfg stores Windows virtual-key numbers. These numeric identifiers are
// portable configuration data; resolving real key events belongs to the host.
inline constexpr unsigned tab = 9, enter = 13, alt = 18, pause = 19, escape = 27;
inline constexpr unsigned left = 37, up = 38, right = 39, down = 40;
inline constexpr unsigned left_windows = 91, right_windows = 92, menu = 93;
inline constexpr unsigned numpad_2 = 98, numpad_8 = 104;
inline constexpr unsigned f1 = 112, f2 = 113, f3 = 114, f4 = 115, f10 = 121;
inline constexpr unsigned space = 32, select = 41, numpad_4 = 100, numpad_6 = 102;
inline constexpr unsigned left_shift = 160, right_shift = 161, left_control = 162, right_control = 163;
} // namespace web::key_code
