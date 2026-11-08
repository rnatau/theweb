#pragma once
#include "../web.hpp"

namespace web {
// Precomputed silhouette samples. Ball drawing intersects these with local
// subpixel visibility from the original depth map, then blends the boundary.
class SpriteCoverage {
public:
    static constexpr unsigned samples = 16;
    struct Pixel {
        int x, y;
        std::uint16_t color;
        std::uint8_t coverage;
        std::uint16_t coverage_mask;
    };
    explicit SpriteCoverage(std::span<const Span>);
    std::span<const Pixel> pixels() const {
        return pixels_;
    }
    void composite(Bitmap&, const TileMap&, int x, int y, int depth_top, int bytes_per_pixel, const std::uint8_t* ball_depth = nullptr) const;

private:
    std::vector<Pixel> pixels_;
};
} // namespace web
