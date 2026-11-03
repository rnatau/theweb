#pragma once
#include "dot_matrix.hpp"

namespace web {
struct DmdLayout {
    int x, y, spacing, dot_width, dot_height;
    int width() const {
        return DotMatrixFrame::width * spacing;
    }
    int height() const {
        return DotMatrixFrame::height * spacing;
    }
};
// Original display placement 0x41c8c0 and indexed/RGB555 raster 0x41b17e.
// Gaps retain the original table/backbox artwork.
class DmdRaster {
public:
    static DmdLayout layout(unsigned variant);
    static void draw(Bitmap& screen, const DotMatrixFrame& frame, unsigned variant);
    // Counter is the original post-increment raster cadence (0x42f4ec).
    static void draw(Bitmap& screen, const DotMatrixFrame& frame, unsigned variant, bool low_detail, std::uint32_t counter);
};
} // namespace web
