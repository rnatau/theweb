#pragma once
#include "../web.hpp"
#include "../input.hpp"
#include "presentation.hpp"
#include <algorithm>

namespace web {
struct NudgeOffset {
    int x = 0, y = 0;
    bool operator==(const NudgeOffset&) const = default;
};
// Translate the completed table viewport, including its crisp DMD. Replicate
// edge pixels to cover exposed strips; letterboxing and diagnostics stay fixed.
// Row order and copy direction preserve overlapping source pixels in place.
inline void present_nudge(std::span<std::uint32_t> pixels, int width, int height, PresentationRect area, NudgeOffset offset) {
    if (width <= 0 || height <= 0 || pixels.size() != std::size_t(width) * height || area.x < 0 || area.y < 0 || area.width <= 0 || area.height <= 0 || area.x > width - area.width ||
        area.y > height - area.height)
        throw std::invalid_argument("Invalid directional nudge viewport");
    if (offset == NudgeOffset{})
        return;
    const int dx = std::clamp(offset.x, 1 - area.width, area.width - 1);
    const int dy = std::clamp(offset.y, 1 - area.height, area.height - 1);
    for (int row = 0; row < area.height; ++row) {
        const int y = dy > 0 ? area.height - 1 - row : row;
        const int source_y = std::clamp(y - dy, 0, area.height - 1);
        const auto source = pixels.begin() + std::size_t(area.y + source_y) * width + area.x;
        const auto target = pixels.begin() + std::size_t(area.y + y) * width + area.x;
        const auto left = *source, right = *(source + area.width - 1);
        if (dx > 0) {
            std::copy_backward(source, source + area.width - dx, target + area.width);
            std::fill_n(target, dx, left);
        } else if (dx < 0) {
            std::copy(source - dx, source + area.width, target);
            std::fill(target + area.width + dx, target + area.width, right);
        } else if (target != source)
            std::copy_n(source, area.width, target);
    }
}
// Main-loop 0x414044: self-blit the current front surface upward. The uncovered
// bottom rows remain unchanged. The next ordinary table submission replaces it.
inline void nudge_feedback(Bitmap& canvas, unsigned variant) {
    const auto distance = 2 * (1 + ((variant & 0x30) >> 4));
    if (canvas.height < int(distance) || canvas.width < 0 || canvas.pixels.size() != std::size_t(canvas.width) * canvas.height)
        throw std::invalid_argument("Invalid nudge feedback canvas");
    const auto begin = canvas.pixels.begin();
    std::copy(begin + std::size_t(distance) * canvas.width, canvas.pixels.end(), begin);
}
} // namespace web
