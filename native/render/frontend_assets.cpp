#include "frontend_assets.hpp"
#include <algorithm>
#include <cstdio>

namespace web {
FrontendImage FrontendAssets::describe(unsigned mode, unsigned image) {
    if (mode > 7 || image >= slide_count + 3)
        throw std::out_of_range("Unsupported original menu image");
    auto format = display_mode(mode << 4);
    std::string filename;
    switch (image) {
        case 0:
            filename = "EMPIRE.BIT";
            break;
        case 1:
            filename = "PROPIN.BIT";
            break;
        case 2:
            filename = "BACKBOX.BIT";
            break;
        default: {
            char name[20]{};
            std::snprintf(name, sizeof(name), "INTRO%02u.BIT", image - 3);
            filename = name;
            if (image > 3) {
                constexpr int heights[] = {346, 432, 554};
                format.height = heights[(mode & 3) - 1];
            }
            break;
        }
    }
    return {std::move(filename), format};
}
Bitmap FrontendAssets::image(unsigned mode, unsigned image) const {
    const auto description = describe(mode, image);
    return load_bitmap(root_ / "FRONTEND" / std::to_string(mode) / description.filename, description.mode);
}
Bitmap FrontendAssets::slide(unsigned index, unsigned mode) const {
    if (index >= slide_count)
        throw std::out_of_range("Unsupported original slideshow image");
    const auto format = display_mode(mode << 4);
    const auto source = image(mode, index + 3);
    if (source.width != format.width || source.height > format.height)
        throw std::runtime_error("Slideshow image does not fit its original display");
    Bitmap canvas{format.width, format.height, std::vector<std::uint32_t>(std::size_t(format.width) * format.height)};
    canvas.palette = source.palette;
    const auto top = (format.height - source.height) / 2;
    std::copy(source.pixels.begin(), source.pixels.end(), canvas.pixels.begin() + std::size_t(top) * canvas.width);
    return canvas;
}
Bytes FrontendAssets::view_icon(unsigned index) const {
    if (index >= view_count)
        throw std::out_of_range("Unsupported original camera icon");
    const auto pixels = read_file(root_ / "FRONTEND/MATRIX" / ("VW" + std::to_string(index + 1) + "ICON.BIT"));
    if (pixels.size() != 32 * 24 || std::any_of(pixels.begin(), pixels.end(), [](auto value) { return value > 3; }))
        throw std::runtime_error("Invalid original camera icon");
    return pixels;
}
} // namespace web
