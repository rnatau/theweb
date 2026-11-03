#pragma once
#include "../web.hpp"

namespace web {
struct FrontendImage {
    std::string filename;
    DisplayMode mode;
};
// Original MENU's raw bitmaps and 32x24 dot-matrix camera icons.
class FrontendAssets {
public:
    explicit FrontendAssets(std::filesystem::path assets) :
        root_(std::move(assets)) {}
    static constexpr unsigned slide_count = 17, view_count = 6;
    static FrontendImage describe(unsigned mode, unsigned image);
    Bitmap image(unsigned mode, unsigned image) const;
    Bitmap slide(unsigned index, unsigned mode = 7) const;
    Bytes view_icon(unsigned index) const;

private:
    std::filesystem::path root_;
};
} // namespace web
