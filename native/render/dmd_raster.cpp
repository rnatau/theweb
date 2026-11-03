#include "dmd_raster.hpp"
#include <stdexcept>

namespace web {
DmdLayout DmdRaster::layout(unsigned variant) {
    display_mode(variant);
    // Original placement ignores the indexed/high-color bit. Cameras zero to
    // two share the horizontal backbox; cameras four to six have angled insets.
    constexpr std::array<std::array<std::array<int, 2>, 4>, 3> origins = {
        {{{{192, 0}, {194, 14}, {195, 11}, {193, 13}}}, {{{272, 8}, {276, 44}, {272, 40}, {274, 42}}}, {{{320, 0}, {321, 34}, {319, 34}, {319, 38}}}}
    };
    const unsigned resolution = (variant >> 4 & 3) - 1;
    const unsigned camera = (variant & 7) < 3 ? 0 : (variant & 7) - 3;
    const auto origin = origins[resolution][camera];
    return {origin[0], origin[1], resolution == 2 ? 3 : 2, resolution == 2 ? 2 : 1, resolution == 2 ? 2 : 1};
}
void DmdRaster::draw(Bitmap& screen, const DotMatrixFrame& frame, unsigned variant) {
    draw(screen, frame, variant, false, 0);
}
void DmdRaster::draw(Bitmap& screen, const DotMatrixFrame& frame, unsigned variant, bool low_detail, std::uint32_t counter) {
    const auto format = display_mode(variant);
    if (screen.width != format.width || screen.height != format.height || screen.pixels.size() != std::size_t(format.width) * format.height)
        throw std::invalid_argument("DMD raster dimensions disagree with the table view");
    const auto placement = layout(variant);
    const std::array origin = {placement.x, placement.y};
    const bool large_dots = placement.spacing == 3;
    const int spacing = placement.spacing;
    const int dot_width = placement.dot_width;
    const int dot_height = large_dots && !low_detail ? 2 : 1;
    // Original immutable DWORD pairs at 0x42ea58. The two brightest logical
    // values intentionally retain their original ordering (2 brighter than 3).
    constexpr std::array<std::uint16_t, 4> rgb555 = {0x1c60, 0x3ce0, 0x7de0, 0x5d60};
    constexpr std::array<unsigned, 4> indexed = {1, 2, 4, 3}; // Original packed pairs at 0x42ea4e.
    const auto expand = [](std::uint16_t color) {
        const auto channel = [](unsigned value) { return ((value & 31) * 255 + 15) / 31; };
        return channel(color >> 10) << 16 | channel(color >> 5) << 8 | channel(color);
    };
    for (int row = 0; row < DotMatrixFrame::height; ++row)
        for (int column = 0; column < DotMatrixFrame::width; ++column) {
            const auto intensity = frame.pixels[std::size_t(row) * DotMatrixFrame::width + column];
            if (intensity >= rgb555.size())
                throw std::out_of_range("Invalid DMD raster intensity");
            if (low_detail && !large_dots && (column & 1) != ((row + ((counter & 2) ? 1 : 0)) & 1))
                continue;
            const int field_offset = large_dots && low_detail && !(counter & 2) ? 1 : 0;
            const auto color = format.bytes_per_pixel == 1 ? screen.palette[indexed[intensity]] : expand(rgb555[intensity]);
            for (int dy = 0; dy < dot_height; ++dy)
                for (int dx = 0; dx < dot_width; ++dx)
                    screen.pixels[std::size_t(origin[1] + row * spacing + field_offset + dy) * screen.width + origin[0] + column * spacing + dx] = color;
        }
}
} // namespace web
