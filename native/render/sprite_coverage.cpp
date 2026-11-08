#include "sprite_coverage.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <limits>
#include <stdexcept>

namespace web {
namespace {
std::uint16_t coverage_mask(unsigned neighborhood) {
    if (!neighborhood)
        return 0;
    if (neighborhood == 0x1ff)
        return 0xffff;
    const auto occupied = [&](unsigned x, unsigned y) { return (neighborhood >> (y * 3 + x)) & 1; };
    std::uint16_t result = 0;
    unsigned sample = 0;
    for (const int sy : {-3, -1, 1, 3})
        for (const int sx : {-3, -1, 1, 3}) {
            const unsigned bx = sx < 0 ? 0 : 1, by = sy < 0 ? 0 : 1;
            const unsigned fx = unsigned(sx < 0 ? sx + 8 : sx), fy = unsigned(sy < 0 ? sy + 8 : sy);
            const auto area = occupied(bx, by) * (8 - fx) * (8 - fy) + occupied(bx + 1, by) * fx * (8 - fy) + occupied(bx, by + 1) * (8 - fx) * fy + occupied(bx + 1, by + 1) * fx * fy;
            if (area >= 32)
                result |= std::uint16_t(1u << sample);
            ++sample;
        }
    return result;
}
std::uint16_t visible_samples(const TileMap& depth, int x, int y, std::uint8_t ball_depth) {
    unsigned neighborhood = 0, bit = 0;
    for (int dy = -1; dy <= 1; ++dy)
        for (int dx = -1; dx <= 1; ++dx) {
            const int column = x + dx, row = y + dy;
            if (column >= 0 && row >= 0 && column < depth.columns * 8 && row < depth.rows * 8 && depth.sample(column, row) >= ball_depth)
                neighborhood |= 1u << bit;
            ++bit;
        }
    return coverage_mask(neighborhood);
}
} // namespace
SpriteCoverage::SpriteCoverage(std::span<const Span> spans) {
    std::int64_t left = std::numeric_limits<int>::max(), top = left, right = std::numeric_limits<int>::min(), bottom = right;
    for (const auto& span : spans)
        if (!span.pixels.empty()) {
            left = std::min(left, std::int64_t(span.x) - 1);
            top = std::min(top, std::int64_t(span.y) - 1);
            right = std::max(right, std::int64_t(span.x) + std::int64_t(span.pixels.size()) + 1);
            bottom = std::max(bottom, std::int64_t(span.y) + 2);
        }
    if (right <= left || bottom <= top)
        return;
    constexpr std::int64_t maximum_mask_pixels = 16 * 1024 * 1024;
    if (left < std::numeric_limits<int>::min() || top < std::numeric_limits<int>::min() || right > std::numeric_limits<int>::max() || bottom > std::numeric_limits<int>::max() ||
        right - left > maximum_mask_pixels || bottom - top > maximum_mask_pixels || (right - left) * (bottom - top) > maximum_mask_pixels)
        throw std::invalid_argument("Sprite coverage mask exceeds supported dimensions");
    const int width = int(right - left), height = int(bottom - top);
    std::vector<std::uint8_t> mask(std::size_t(width) * height);
    std::vector<std::uint16_t> colors(mask.size());
    for (const auto& span : spans)
        for (std::size_t i = 0; i < span.pixels.size(); ++i) {
            const auto index = std::size_t(std::int64_t(span.y) - top) * width + std::size_t(std::int64_t(span.x) - left) + i;
            mask[index] = 1;
            colors[index] = span.pixels[i];
        }
    const auto occupied = [&](int x, int y) { return x >= 0 && y >= 0 && x < width && y < height ? unsigned(mask[std::size_t(y) * width + x]) : 0u; };
    for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x) {
            unsigned neighborhood = 0, bit = 0;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    neighborhood |= occupied(x + dx, y + dy) << bit++;
            if (!neighborhood)
                continue;
            // Retain individual subpixel samples so an occluder can intersect
            // the silhouette without multiplying two unrelated opacity values.
            const auto subpixels = coverage_mask(neighborhood);
            const auto coverage = unsigned(std::popcount(subpixels));
            if (!coverage)
                continue;
            auto color = colors[std::size_t(y) * width + x];
            if (!occupied(x, y)) {
                // Extend an existing sprite color into its transparent fringe;
                // transparent texels must never contribute black outlines.
                constexpr std::array<std::array<int, 2>, 8> neighbors = {
                    {{0, -1}, {-1, 0}, {1, 0}, {0, 1}, {-1, -1}, {1, -1}, {-1, 1}, {1, 1}}
                };
                for (const auto& delta : neighbors)
                    if (occupied(x + delta[0], y + delta[1])) {
                        color = colors[std::size_t(y + delta[1]) * width + x + delta[0]];
                        break;
                    }
            }
            pixels_.push_back({int(left + x), int(top + y), color, std::uint8_t(coverage), subpixels});
        }
}
void SpriteCoverage::composite(Bitmap& target, const TileMap& depth, int x, int y, int depth_top, int bytes_per_pixel, const std::uint8_t* ball_depth) const {
    for (const auto& pixel : pixels_) {
        const auto column = std::int64_t(x) + pixel.x, row = std::int64_t(y) + pixel.y;
        if (column < 0 || column >= target.width || row < 0 || row >= target.height)
            continue;
        if (ball_depth && (row < depth_top || row >= std::int64_t(depth_top) + depth.rows * 8))
            continue;
        const auto coverage =
            ball_depth ? unsigned(std::popcount(std::uint16_t(pixel.coverage_mask & visible_samples(depth, int(column), int(row) - depth_top, *ball_depth)))) : unsigned(pixel.coverage);
        if (!coverage)
            continue;
        auto& destination = target.pixels[std::size_t(row) * target.width + std::size_t(column)];
        const auto value = pixel.color;
        const std::uint32_t source =
            bytes_per_pixel == 2 ? ((((value >> 10 & 31) * 255 + 15) / 31) << 16 | (((value >> 5 & 31) * 255 + 15) / 31) << 8 | ((value & 31) * 255 + 15) / 31) : target.palette.at(value);
        if (coverage == samples)
            destination = source;
        else {
            std::uint32_t blended = 0;
            for (const unsigned shift : {0u, 8u, 16u})
                blended |= ((((source >> shift) & 255) * coverage + ((destination >> shift) & 255) * (samples - coverage) + samples / 2) / samples) << shift;
            destination = blended;
        }
    }
}
} // namespace web
