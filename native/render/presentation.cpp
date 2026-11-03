#include "presentation.hpp"
#include <algorithm>
#include <stdexcept>

namespace web {
PresentationRect presentation_viewport(int sw, int sh, int cw, int ch, PresentationScaling scaling) {
    if (sw <= 0 || sh <= 0 || cw <= 0 || ch <= 0)
        throw std::invalid_argument("Presentation dimensions must be positive");
    int width, height;
    const int scale = std::min(cw / sw, ch / sh);
    if (scaling == PresentationScaling::integer && scale > 0) {
        width = sw * scale;
        height = sh * scale;
    } else if (std::int64_t(cw) * sh <= std::int64_t(ch) * sw) {
        width = cw;
        height = std::max(1, int(std::int64_t(cw) * sh / sw));
    } else {
        height = ch;
        width = std::max(1, int(std::int64_t(ch) * sw / sh));
    }
    return {(cw - width) / 2, (ch - height) / 2, width, height};
}
bool integer_presentation(const PresentationRect& area, int sw, int sh) {
    return sw > 0 && sh > 0 && area.width >= sw && area.height >= sh && area.width % sw == 0 && area.height % sh == 0 && area.width / sw == area.height / sh;
}
std::optional<PresentedDmd> presented_dmd(const PresentationRect& view, int sw, int sh, DmdLayout native) {
    if (sw <= 0 || sh <= 0 || view.width <= 0 || view.height <= 0 || native.spacing <= 0)
        throw std::invalid_argument("Invalid DMD presentation dimensions");
    const auto project_x = [&](int x) { return view.x + int(std::int64_t(x) * view.width / sw); };
    const auto project_y = [&](int y) { return view.y + int(std::int64_t(y) * view.height / sh); };
    const int left = project_x(native.x), top = project_y(native.y);
    const int width = project_x(native.x + native.width()) - left, height = project_y(native.y + native.height()) - top;
    const int pitch = std::min(width / DotMatrixFrame::width, height / DotMatrixFrame::height);
    if (!pitch)
        return {};
    const auto size = [&](int source) { return std::clamp((pitch * source + native.spacing / 2) / native.spacing, 1, std::max(1, pitch - 1)); };
    return PresentedDmd{left + (width - pitch * DotMatrixFrame::width) / 2, top + (height - pitch * DotMatrixFrame::height) / 2, pitch, size(native.dot_width), size(native.dot_height)};
}
void draw_presented_dmd(std::span<std::uint32_t> target, int width, int height, const Bitmap& source, DmdLayout native, PresentedDmd output) {
    if (width <= 0 || height <= 0 || target.size() != std::size_t(width) * height || source.pixels.size() != std::size_t(source.width) * source.height || native.x < 0 || native.y < 0 ||
        native.x + native.width() > source.width || native.y + native.height() > source.height || output.pitch <= 0 || output.dot_width <= 0 || output.dot_width > output.pitch ||
        output.dot_height <= 0 || output.dot_height > output.pitch || output.x < 0 || output.y < 0 || output.x + output.pitch * DotMatrixFrame::width > width ||
        output.y + output.pitch * DotMatrixFrame::height > height)
        throw std::invalid_argument("DMD presentation exceeds its surface");
    for (int row = 0; row < DotMatrixFrame::height; ++row)
        for (int column = 0; column < DotMatrixFrame::width; ++column)
            for (int y = 0; y < output.dot_height; ++y)
                for (int x = 0; x < output.dot_width; ++x) {
                    const int sx = native.x + column * native.spacing + x * native.dot_width / output.dot_width;
                    const int sy = native.y + row * native.spacing + y * native.dot_height / output.dot_height;
                    target[std::size_t(output.y + row * output.pitch + y) * width + output.x + column * output.pitch + x] = source.pixels[std::size_t(sy) * source.width + sx];
                }
}
} // namespace web
