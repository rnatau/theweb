#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include "slideshow_window.hpp"
#include "frontend_window.hpp"
#include "../../render/frontend_assets.hpp"
#include "../../render/slideshow.hpp"
#include "../../render/frontend_input.hpp"

namespace web {
namespace {
unsigned slideshow_mode(const GameOptions& options) {
    constexpr std::array<unsigned, 6> modes = {1, 2, 3, 5, 6, 7};
    return modes.at(options.screen_mode() - 1);
}
class SlideshowWindow : public FrontendWindow {
public:
    SlideshowWindow(const std::filesystem::path& assets, const GameOptions& options) :
        FrontendWindow(FrontendAssets(assets).slide(0, slideshow_mode(options)), L"TheWebOriginalSlideshow", L"Original slideshow"),
        assets_(assets),
        options_(options) {
        slideshow_.step(0);
        update_caption();
    }

private:
    FrontendAssets assets_;
    GameOptions options_;
    Slideshow slideshow_;
    std::uint32_t edges_ = 0;
    void update_caption() {
        caption(L"Original slideshow | " + std::to_wstring(slideshow_.image() + 1) + L"/17 | Flippers / arrows: browse | Esc: back");
    }
    void tick() override {
        const auto image = slideshow_.step(edges_);
        edges_ = 0;
        if (slideshow_.finished()) {
            close();
            return;
        }
        if (image) {
            canvas() = assets_.slide(*image, slideshow_mode(options_));
            update_caption();
            refresh();
        }
    }
    void key(unsigned value) override {
        if (value == VK_ESCAPE)
            close();
        else if (value == options_.key(0))
            edges_ |= 1;
        else if (value == options_.key(1))
            edges_ |= 2;
        else
            edges_ |= frontend_key_action(value, true) & 7;
    }
    void blur() override {
        edges_ = 0;
    }
    std::optional<std::intptr_t> probe(unsigned id) const override {
        switch (id) {
            case 1:
                return slideshow_.image();
            case 2:
                return slideshow_.automatic();
            case 4:
                return slideshow_.remaining();
            default:
                return {};
        }
    }
};
} // namespace
void show_slideshow(void* owner, const std::filesystem::path& assets, const GameOptions& options) {
    SlideshowWindow(assets, options).run(owner);
}
} // namespace web
