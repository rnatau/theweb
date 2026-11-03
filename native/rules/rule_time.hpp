#pragma once
#include <cstdint>

namespace web::rules::rule_time {
// Original rule timers count 32768 units per nominal second. Keep integer
// arithmetic: a 60 Hz update subtracts second / 60 (546), without rounding.
inline constexpr std::int32_t second = 0x8000;
inline constexpr std::int32_t eighth_second = second / 8;
inline constexpr std::int32_t quarter_second = second / 4;
inline constexpr std::int32_t half_second = second / 2;
inline constexpr std::int32_t three_quarter_second = 3 * second / 4;
} // namespace web::rules::rule_time
