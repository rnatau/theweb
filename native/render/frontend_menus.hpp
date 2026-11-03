#pragma once
#include "backbox_menu.hpp"
#include "camera_menu.hpp"
#include "../options.hpp"
#include <memory>

namespace web {
// MENU's main/options dispatcher (0x4040d1..0x4046ed). Each list retains
// selection, blink and scrolling state while another list is visible.
class FrontendMenus {
public:
    enum class Action {
        none,
        play,
        slideshow,
        quit,
        fullscreen
    };
    FrontendMenus(const DotMatrixRenderer&, const FrontendAssets&, GameOptions&, std::span<const BackboxMenu::Item> option_items = {}, std::span<const BackboxMenu::Item> main_items = {});
    virtual ~FrontendMenus() = default;
    virtual bool input(std::uint32_t edges, SoundSink* sounds);
    virtual bool activation_complete() const;
    virtual Action accept();
    virtual Action escape();
    virtual DotMatrixFrame step(const DotMatrixFrame&, std::int32_t elapsed);
    virtual std::int32_t selected_id() const;
    virtual unsigned first_visible() const;
    virtual std::uint32_t counter() const;
    virtual std::int32_t activation_remaining() const;
    virtual bool capture_key(unsigned) {
        return false;
    }
    virtual bool capture_gamepad(std::uint64_t) {
        return false;
    }
    virtual bool capturing() const {
        return false;
    }
    virtual unsigned page() const {
        return page_;
    }

private:
    // The first six values index BackboxMenu::List. Camera has its own widget.
    enum Page : unsigned {
        main,
        options,
        screen_mode,
        screen_flip,
        music_restart,
        graphic_detail,
        camera
    };
    GameOptions& options_;
    std::array<std::unique_ptr<BackboxMenu>, 6> lists_;
    CameraMenu camera_;
    Page page_ = Page::main;
    void remember();
    void restore();
};
} // namespace web
