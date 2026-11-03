#pragma once
#include "../web.hpp"

namespace web {
// Original six-bit palette nearest-color shadow remap, initialized at 0x416268.
class IndexedShadow {
public:
    explicit IndexedShadow(const std::array<std::uint32_t, 256>& palette);
    static std::array<std::uint8_t, 256> remap(std::span<const std::uint8_t> palette);
    std::uint32_t shade(std::uint32_t color) const;

private:
    std::array<std::pair<std::uint32_t, std::uint32_t>, 256> colors_;
};
} // namespace web
