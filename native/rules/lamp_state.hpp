#pragma once
#include <array>
#include <cstdint>
namespace web::rules {
struct LampState {
    std::array<std::int32_t, 79> outputs{};
    std::array<std::int32_t, 6> drop_targets{};
    std::array<std::uint8_t, 2048> animation_cells{};
    std::array<std::uint32_t, 70> sample_indices{};
};
struct LampFlash {
    std::int32_t remaining = 0, output = 0;
};
} // namespace web::rules
