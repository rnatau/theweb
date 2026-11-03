#include "camera_menu.hpp"
#include "../physics/integer.hpp"

namespace web {
namespace {
constexpr unsigned visible_icons = 3;
constexpr int icon_width = 32, icon_height = 24, icon_stride = 35;
// Original first/last horizontal indicator pixels at MENU 0x415ca8/0x415c68.
constexpr std::array<std::uint8_t, 8> first_arrow = {0x03, 0x0f, 0x3f, 0xff, 0xff, 0x3f, 0x0f, 0x03};
constexpr std::array<std::uint8_t, 8> last_arrow = {0xc0, 0xf0, 0xfc, 0xff, 0xff, 0xfc, 0xf0, 0xc0};
void arrow(DotMatrixFrame& frame, int x, const std::array<std::uint8_t, 8>& pixels) {
    for (int row = 0; row < 8; ++row)
        for (int column = 0; column < 8; ++column)
            if (pixels[row] & (0x80 >> column))
                frame.pixels[std::size_t(12 + row) * DotMatrixFrame::width + x + column] = 2;
}
} // namespace
CameraMenu::CameraMenu(const FrontendAssets& assets) {
    for (unsigned i = 0; i < icons_.size(); ++i)
        icons_[i] = assets.view_icon(i);
}
void CameraMenu::fit_selection() {
    const auto selected = unsigned(*selection_.selected_index());
    if (selected < first_visible_)
        first_visible_ = selected;
    else if (selected >= first_visible_ + visible_icons)
        first_visible_ = selected - visible_icons + 1;
}
void CameraMenu::next() {
    selection_.next();
    fit_selection();
}
void CameraMenu::previous() {
    selection_.previous();
    fit_selection();
}
void CameraMenu::select(std::int32_t id) {
    selection_.select(id);
    fit_selection();
}
void CameraMenu::restore() {
    selection_.restore();
    fit_selection();
}
bool CameraMenu::input(std::uint32_t edges, SoundSink* sounds) {
    if (edges & input_action::both_flippers) {
        const bool left = (edges & input_action::left_flipper) != 0;
        if (sounds)
            sounds->sound({174, left ? -64 : 64, 0, 1});
        if (left)
            previous();
        else
            next();
    } else if (edges & input_action::launch) {
        activate();
        if (sounds)
            sounds->sound({181, 0, 0, 1});
        return true;
    }
    return false;
}
DotMatrixFrame CameraMenu::step(const DotMatrixFrame& backdrop, std::int32_t elapsed) {
    counter_ += std::uint32_t(elapsed);
    auto frame = backdrop;
    const auto selected = unsigned(*selection_.selected_index());
    for (unsigned column = 0; column < visible_icons && first_visible_ + column < icons_.size(); ++column) {
        const auto index = first_visible_ + column;
        const int x = 12 + int(column) * icon_stride;
        if (index != selected || (counter_ & 3))
            for (int row = 0; row < icon_height; ++row)
                for (int dx = 0; dx < icon_width; ++dx) {
                    const auto value = icons_[index][std::size_t(row) * icon_width + dx];
                    if (value)
                        frame.pixels[std::size_t(4 + row) * DotMatrixFrame::width + x + dx] = value;
                }
        if (index == selected && activation_remaining_ > 0) {
            activation_remaining_ = physics::integer::sub(activation_remaining_, elapsed);
            if (std::uint32_t(activation_remaining_) & 1)
                for (int row = 1; row < 31; ++row)
                    for (int dx = 0; dx < icon_width; ++dx)
                        frame.pixels[std::size_t(row) * DotMatrixFrame::width + x + dx] ^= 2;
        }
    }
    if (first_visible_ + visible_icons < icons_.size())
        arrow(frame, 119, last_arrow);
    if (first_visible_)
        arrow(frame, 1, first_arrow);
    return frame;
}
} // namespace web
