#include "../render/presentation.hpp"
#include "../render/nudge_feedback.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
    ++checks;
}
} // namespace
int main() {
    try {
        using namespace web;
        // Independent out-of-place sampling checks the overlapping in-place
        // translation, including edge extension and fixed letterbox pixels.
        const PresentationRect table{2, 2, 7, 5};
        std::vector<std::uint32_t> original(11 * 9);
        for (std::size_t i = 0; i < original.size(); ++i)
            original[i] = std::uint32_t(i + 1);
        for (int dy = -6; dy <= 6; ++dy)
            for (int dx = -8; dx <= 8; ++dx) {
                auto shifted = original;
                present_nudge(shifted, 11, 9, table, {dx, dy});
                for (int y = 0; y < 9; ++y)
                    for (int x = 0; x < 11; ++x) {
                        int sx = x, sy = y;
                        if (x >= table.x && x < table.x + table.width && y >= table.y && y < table.y + table.height) {
                            sx = table.x + std::clamp(x - table.x - std::clamp(dx, 1 - table.width, table.width - 1), 0, table.width - 1);
                            sy = table.y + std::clamp(y - table.y - std::clamp(dy, 1 - table.height, table.height - 1), 0, table.height - 1);
                        }
                        check(shifted[y * 11 + x] == original[sy * 11 + sx], "Nudge translation corrupted overlapping pixels or the letterbox");
                    }
            }
        check(presentation_viewport(1024, 768, 1920, 1080, PresentationScaling::fit) == PresentationRect{240, 0, 1440, 1080}, "1080p fit changes the aspect ratio");
        check(presentation_viewport(1024, 768, 3840, 2160, PresentationScaling::fit) == PresentationRect{480, 0, 2880, 2160}, "4K fit changes the aspect ratio");
        check(presentation_viewport(1024, 768, 1920, 1080, PresentationScaling::integer) == PresentationRect{448, 156, 1024, 768}, "Integer presentation must retain exact pixels");
        check(presentation_viewport(640, 480, 1920, 1080, PresentationScaling::integer) == PresentationRect{320, 60, 1280, 960}, "640 presentation must use the largest fitting integer");
        bool rejected = false;
        try {
            presentation_viewport(0, 768, 1920, 1080, PresentationScaling::fit);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "Invalid source dimensions were accepted");
        constexpr std::uint32_t backdrop = 0x102030;
        for (const unsigned mode : {0x10u, 0x20u, 0x30u, 0x50u, 0x60u, 0x70u})
            for (const unsigned camera : {0u, 1u, 2u, 4u, 5u, 6u}) {
                const auto variant = mode | camera;
                const auto format = display_mode(variant);
                Bitmap source{format.width, format.height, {}};
                source.pixels.assign(std::size_t(source.width) * source.height, backdrop);
                source.palette[1] = 0x100000;
                source.palette[2] = 0x400000;
                source.palette[3] = 0x800000;
                source.palette[4] = 0xff0000;
                DotMatrixFrame frame;
                for (std::size_t i = 0; i < frame.pixels.size(); ++i)
                    frame.pixels[i] = (i + i / 128) % 4;
                DmdRaster::draw(source, frame, variant);
                const auto area = DmdRaster::layout(variant);
                const auto native_view = presentation_viewport(source.width, source.height, source.width, source.height, PresentationScaling::fit);
                check(integer_presentation(native_view, source.width, source.height), "Native size was classified as fractional");
                Bitmap exact{source.width, source.height, {}};
                exact.pixels.assign(source.pixels.size(), backdrop);
                draw_presented_dmd(exact.pixels, exact.width, exact.height, source, area, *presented_dmd(native_view, source.width, source.height, area));
                check(exact.pixels == source.pixels, "Native DMD colors, positions or gaps changed");
                for (const auto size : {
                         std::array{1920, 1080},
                         std::array{2560, 1440},
                         std::array{3440, 1440},
                         std::array{3840, 2160},
                         std::array{1080, 1920},
                         std::array{320,  200 }
                }) {
                    const auto view = presentation_viewport(source.width, source.height, size[0], size[1], PresentationScaling::fit);
                    check(view.x >= 0 && view.y >= 0 && view.x + view.width <= size[0] && view.y + view.height <= size[1], "Fit cropped the table");
                    const auto dots = presented_dmd(view, source.width, source.height, area);
                    if (!dots)
                        continue;
                    std::vector<std::uint32_t> output(static_cast<std::size_t>(size[0]) * size[1], backdrop);
                    draw_presented_dmd(output, size[0], size[1], source, area, *dots);
                    for (int row = 0; row < 32; ++row)
                        for (int col = 0; col < 128; ++col) {
                            const auto color = source.pixels[std::size_t(area.y + row * area.spacing) * source.width + area.x + col * area.spacing];
                            for (int y = 0; y < dots->dot_height; ++y)
                                for (int x = 0; x < dots->dot_width; ++x)
                                    check(output[std::size_t(dots->y + row * dots->pitch + y) * size[0] + dots->x + col * dots->pitch + x] == color, "Scaled DMD blurred or changed a dot color");
                            if (dots->pitch > dots->dot_width)
                                check(output[std::size_t(dots->y + row * dots->pitch) * size[0] + dots->x + col * dots->pitch + dots->dot_width] == backdrop, "DMD dot gap was blurred away");
                        }
                }
            }
        std::cout << checks << " presentation geometry/pixel checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
