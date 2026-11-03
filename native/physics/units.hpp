#pragma once
#include <cstdint>

namespace web::physics::fixed_point {
// Positions use 16 fractional bits; sine/cosine lookup results use 15.
// Neither scale is the rule timer's unit, even when a literal looks identical.
inline constexpr unsigned position_fraction_bits = 16;
inline constexpr std::int32_t position_one = 1 << position_fraction_bits;
inline constexpr std::int32_t trig_one = 1 << 15;
} // namespace web::physics::fixed_point

namespace web::physics::angle {
// One full turn wraps in an unsigned 16-bit angle. Zero points along +Y.
inline constexpr std::int32_t full_turn = 0x10000, half_turn = full_turn / 2;
inline constexpr std::int32_t quarter_turn = full_turn / 4, eighth_turn = full_turn / 8;
} // namespace web::physics::angle

namespace web::physics::table_geometry {
inline constexpr int tile_side = 8, columns = 40, rows = 77;
inline constexpr int width = columns * tile_side, height = rows * tile_side;
// Map wrappers invert Y around 614. The two end samples are -1 and 614.
inline constexpr int maximum_y = height - 2, minimum_y = -1;
} // namespace web::physics::table_geometry
