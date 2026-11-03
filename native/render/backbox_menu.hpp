#pragma once
#include "dot_matrix.hpp"
#include "menu_selection.hpp"

namespace web {
// Original MENU main list (0x4025b0) and its 126x30-dot vertical presentation.
class BackboxMenu {
public:
    struct Item {
        std::int32_t id;
        std::string_view label;
    };
    enum class List {
        main,
        options,
        screen_mode,
        screen_flip,
        music_restart,
        graphic_detail
    };
    enum class Choice : std::int32_t {
        play = 101,
        slideshow = 102,
        options = 103,
        quit = 104
    };
    explicit BackboxMenu(const DotMatrixRenderer& renderer, List list = List::main);
    BackboxMenu(const DotMatrixRenderer& renderer, std::span<const Item> items);
    void next();
    void previous();
    void select(Choice);
    void select(std::int32_t);
    void disabled(std::int32_t id, bool value);
    void remember() {
        selection_.remember();
    }
    void restore();
    bool input(std::uint32_t edges, SoundSink* sounds = nullptr); // Main-loop left/right/select priority and cues.
    void activate() {
        activation_remaining_ = 4;
    } // MENU 0x402258.
    bool activation_complete() const {
        return activation_remaining_ < 1;
    }
    DotMatrixFrame step(const DotMatrixFrame& backdrop, std::int32_t elapsed = 1, int origin_y = 1);
    static void raster(Bitmap& backbox, const DotMatrixFrame& frame); // MENU 0x40ab32, mode 0x51, pattern zero.
    static void prepare(Bitmap& backbox);                             // Main caller's 0x4050e0 clear at (188,369), 262x69.
    const MenuSelection& selection() const {
        return selection_;
    }
    unsigned first_visible() const {
        return first_visible_;
    }
    std::uint32_t counter() const {
        return counter_;
    }
    std::int32_t activation_remaining() const {
        return activation_remaining_;
    }

private:
    const DotMatrixRenderer& renderer_;
    std::vector<std::string_view> labels_;
    std::vector<bool> disabled_;
    std::vector<std::int32_t> ids_;
    MenuSelection selection_;
    unsigned first_visible_ = 0;
    std::uint32_t counter_ = 0;
    std::int32_t activation_remaining_ = 0;
    void fit_selection();
};
} // namespace web
