#include "backbox_menu.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web {
namespace {
constexpr int origin_x = 1, text_width = 115, row_height = 8, row_stride = 10;
constexpr unsigned visible_rows = 3;
// Original eight-by-eight arrow pixels at MENU 0x415be8 / 0x415c28,
// represented as row masks. Zero sprite pixels retain the backdrop.
constexpr std::array<std::uint8_t, 8> up_arrow = {0x18, 0x18, 0x3c, 0x3c, 0x7e, 0x7e, 0xff, 0xff};
void arrow(DotMatrixFrame& frame, int y, bool down) {
    constexpr int x = 119;
    for (int row = 0; row < 8; ++row)
        for (int column = 0; column < 8; ++column)
            if (up_arrow[down ? 7 - row : row] & (0x80 >> column))
                frame.pixels[std::size_t(y + row) * DotMatrixFrame::width + x + column] = 2;
}
} // namespace
BackboxMenu::BackboxMenu(const DotMatrixRenderer& renderer, List list) :
    renderer_(renderer),
    selection_({}) {
    switch (list) {
        case List::main:
            labels_ = {"PLAY", "OPTIONS", "SLIDESHOW", "QUIT"};
            ids_ = {101, 103, 102, 104};
            break;
        case List::options:
            labels_ = {"SCREEN MODE", "SCREEN FLIP", "GRAPHIC DETAIL", "TABLE VIEW", "MUSIC RESTART", "MAIN MENU"};
            ids_ = {201, 202, 203, 204, 205, 206};
            break;
        case List::screen_mode:
            labels_ = {"640X480X256", "800X600X256", "1024X768X256", "640X480X32K", "800X600X32K", "1024X768X32K"};
            ids_ = {301, 302, 303, 304, 305, 306};
            break;
        case List::screen_flip:
            labels_ = {"FLIP ENABLED", "FLIP DISABLED"};
            ids_ = {351, 352};
            break;
        case List::music_restart:
            labels_ = {"INSTANT RESTART", "QUICK RESTART", "SLOW RESTART"};
            ids_ = {501, 502, 503};
            break;
        case List::graphic_detail:
            labels_ = {"LOW", "NORMAL", "HIGH"};
            ids_ = {601, 602, 603};
            break;
    }
    disabled_.resize(ids_.size());
    selection_ = MenuSelection(ids_);
}
BackboxMenu::BackboxMenu(const DotMatrixRenderer& renderer, std::span<const Item> items) :
    renderer_(renderer),
    selection_({}) {
    if (items.empty())
        throw std::invalid_argument("A menu needs at least one item");
    for (const auto& item : items) {
        ids_.push_back(item.id);
        labels_.push_back(item.label);
    }
    disabled_.resize(ids_.size());
    selection_ = MenuSelection(ids_);
}
void BackboxMenu::disabled(std::int32_t id, bool value) {
    for (std::size_t i = 0; i < ids_.size(); ++i)
        if (ids_[i] == id)
            disabled_[i] = value;
}
void BackboxMenu::fit_selection() {
    const auto selected = unsigned(*selection_.selected_index());
    if (selected < first_visible_)
        first_visible_ = selected;
    else if (selected >= first_visible_ + visible_rows)
        first_visible_ = selected - visible_rows + 1;
}
void BackboxMenu::next() {
    selection_.next();
    fit_selection();
}
void BackboxMenu::previous() {
    selection_.previous();
    fit_selection();
}
void BackboxMenu::select(Choice choice) {
    select(std::int32_t(choice));
}
void BackboxMenu::select(std::int32_t id) {
    selection_.select(id);
    fit_selection();
}
void BackboxMenu::restore() {
    selection_.restore();
    fit_selection();
}
bool BackboxMenu::input(std::uint32_t edges, SoundSink* sounds) {
    if (edges & input_action::both_flippers) {
        const bool left = (edges & input_action::left_flipper) != 0;
        if (sounds)
            sounds->sound({174, left ? -64 : 64, 0, 1});
        if (left)
            previous();
        else
            next();
    } else if (edges & input_action::launch) {
        const bool enabled = !disabled_[*selection_.selected_index()];
        if (enabled)
            activate();
        if (sounds)
            sounds->sound({181, 0, 0, 1});
        return enabled;
    }
    return false;
}
DotMatrixFrame BackboxMenu::step(const DotMatrixFrame& backdrop, std::int32_t elapsed, int origin_y) {
    counter_ += std::uint32_t(elapsed);
    auto frame = backdrop;
    const auto selected = unsigned(*selection_.selected_index());
    const int available_width = labels_.size() > visible_rows ? text_width : 126;
    for (unsigned row = 0; row < visible_rows && first_visible_ + row < labels_.size(); ++row) {
        const auto index = first_visible_ + row;
        const int y = origin_y + int(row) * row_stride;
        const auto label = labels_[index];
        if (index != selected || (counter_ & 3)) {
            // MENU 0x401564 measures full font cells, although its glyph
            // blitter advances only half a cell for spaces and commas.
            const auto width = int(label.size()) * 8;
            const int x = origin_x + (available_width - width) / 2;
            if (!disabled_[index])
                renderer_.text(frame, label, 1, x, y);
            else {
                DotMatrixFrame glyphs;
                renderer_.text(glyphs, label, 1, x, y);
                for (std::size_t i = 0; i < frame.pixels.size(); ++i)
                    if (glyphs.pixels[i] == 2)
                        frame.pixels[i] = 3;
            }
        }
        if (index == selected && activation_remaining_ > 0) {
            activation_remaining_ = physics::integer::sub(activation_remaining_, elapsed);
            if (std::uint32_t(activation_remaining_) & 1)
                for (int dy = 0; dy < row_height; ++dy)
                    for (int x = origin_x; x < origin_x + available_width; ++x)
                        frame.pixels[std::size_t(y + dy) * DotMatrixFrame::width + x] ^= 2;
        }
    }
    if (first_visible_ + visible_rows < labels_.size())
        arrow(frame, 23, true);
    if (first_visible_)
        arrow(frame, 1, false);
    return frame;
}
void BackboxMenu::raster(Bitmap& backbox, const DotMatrixFrame& frame) {
    constexpr int width = 640, height = 480, origin_x = 192, origin_y = 372, spacing = 2;
    if (backbox.width != width || backbox.height != height || backbox.pixels.size() != width * height)
        throw std::invalid_argument("Original backbox raster requires a 640x480 canvas");
    constexpr std::array<std::uint16_t, 4> colors = {0x1c60, 0x3ce0, 0x7de0, 0x5d60};
    const auto expand = [](std::uint16_t color) {
        const auto channel = [](unsigned value) { return ((value & 31) * 255 + 15) / 31; };
        return channel(color >> 10) << 16 | channel(color >> 5) << 8 | channel(color);
    };
    for (int y = 0; y < DotMatrixFrame::height; ++y)
        for (int x = 0; x < DotMatrixFrame::width; ++x) {
            const auto value = frame.pixels[std::size_t(y) * DotMatrixFrame::width + x];
            if (value >= colors.size())
                throw std::out_of_range("Invalid backbox dot intensity");
            backbox.pixels[std::size_t(origin_y + spacing * y) * width + origin_x + spacing * x] = expand(colors[value]);
        }
}
void BackboxMenu::prepare(Bitmap& backbox) {
    if (backbox.width != 640 || backbox.height != 480 || backbox.pixels.size() != 640 * 480)
        throw std::invalid_argument("Original backbox preparation requires a 640x480 canvas");
    for (int y = 369; y < 369 + 69; ++y)
        std::fill_n(backbox.pixels.begin() + y * 640 + 188, 262, 0);
}
} // namespace web
