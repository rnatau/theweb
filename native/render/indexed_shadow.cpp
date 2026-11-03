#include "indexed_shadow.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace web {
std::array<std::uint8_t, 256> IndexedShadow::remap(std::span<const std::uint8_t> palette) {
    if (palette.size() != 768 || std::any_of(palette.begin(), palette.end(), [](auto channel) { return channel > 63; }))
        throw std::invalid_argument("Indexed shadow needs a 256-color six-bit palette");
    std::array<std::uint8_t, 256> result{};
    for (unsigned source = 0; source < result.size(); ++source) {
        int closest = std::numeric_limits<int>::max();
        for (unsigned candidate = 0; candidate < result.size(); ++candidate) {
            int distance = 0;
            for (unsigned channel = 0; channel < 3; ++channel) {
                const int difference = palette[source * 3 + channel] * 3 / 4 - palette[candidate * 3 + channel];
                distance += difference * difference;
            }
            if (distance < closest) {
                closest = distance;
                result[source] = std::uint8_t(candidate);
                if (!distance)
                    break;
            }
        }
    }
    return result;
}
IndexedShadow::IndexedShadow(const std::array<std::uint32_t, 256>& palette) {
    std::array<std::uint8_t, 768> raw{};
    for (unsigned index = 0; index < palette.size(); ++index)
        for (unsigned channel = 0; channel < 3; ++channel)
            raw[index * 3 + channel] = std::uint8_t((palette[index] >> (18 - channel * 8)) & 63);
    const auto indices = remap(raw);
    for (unsigned index = 0; index < palette.size(); ++index)
        colors_[index] = {palette[index], palette[indices[index]]};
    // Duplicate source colors have identical targets, including the first-tie
    // rule. The renderer can therefore look up expanded RGB without retaining
    // a second full indexed surface.
    std::sort(colors_.begin(), colors_.end());
}
std::uint32_t IndexedShadow::shade(std::uint32_t color) const {
    const auto found = std::lower_bound(colors_.begin(), colors_.end(), color, [](const auto& entry, auto value) { return entry.first < value; });
    if (found == colors_.end() || found->first != color)
        throw std::out_of_range("Indexed shadow source color is outside its table palette");
    return found->second;
}
} // namespace web
