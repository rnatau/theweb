#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "camera_window.hpp"
#include "../../edition.hpp"
#include "frontend_window.hpp"
#include "../../render/backbox_menu.hpp"
#include "../../render/camera_menu.hpp"
#include "../../render/frontend_input.hpp"
#include "sound_output.hpp"
#include <algorithm>
#include <iostream>
#include <utility>

namespace web {
namespace {
constexpr std::array<unsigned, 6> views = {2, 1, 0, 6, 5, 4};
class CameraWindow : public FrontendWindow {
public:
    CameraWindow(const std::filesystem::path& assets, const GameOptions& options, bool forced_off) :
        FrontendWindow(FrontendAssets(assets).image(5, 2), L"TheWebOriginalCamera", L"Original table views | Flippers / arrows: choose | Enter: accept | Esc: cancel"),
        options_(options),
        background_(canvas()),
        renderer_(assets),
        menu_(FrontendAssets(assets)) {
        BackboxMenu::prepare(background_);
        const auto view = std::find(views.begin(), views.end(), options.view());
        if (view != views.end())
            menu_.select(401 + std::int32_t(view - views.begin()));
        draw(0);
        if (!forced_off && options.sound_enabled() && options.sound_channels()) {
            try {
                audio_ = std::make_unique<SoundOutput>(
                    assets,
                    options.sound_rate(),
                    options.sound_channels(),
                    SoundPcmFormat{options.sound_stereo(), options.sound_16bit()},
                    selected_edition().normalize_sound);
                audio_->volume(options.volume());
                audio_->pause(true);
            } catch (const std::exception& error) {
                audio_error(error);
            }
        }
    }
    std::optional<unsigned> result() const {
        return result_;
    }

private:
    const GameOptions& options_;
    Bitmap background_;
    DotMatrixRenderer renderer_;
    CameraMenu menu_;
    unsigned grid_ = 0, last_grid_ = 0, display_accumulator_ = 0;
    std::uint32_t edges_ = 0;
    bool pending_ = false;
    std::optional<unsigned> result_;
    std::unique_ptr<SoundOutput> audio_;
    void audio_error(const std::exception& error) {
        std::cerr << error.what() << " | continuing without camera-menu audio\n";
        audio_.reset();
    }
    void activity(bool active) override {
        if (audio_)
            try {
                audio_->pause(!active);
            } catch (const std::exception& error) {
                audio_error(error);
            }
    }
    void blur() override {
        edges_ = 0;
    }
    void draw(int elapsed) {
        canvas() = background_;
        last_grid_ = grid_;
        BackboxMenu::raster(canvas(), menu_.step(renderer_.grid_frame(last_grid_), elapsed));
        grid_ += unsigned(elapsed);
        if (grid_ > 3)
            grid_ = 0;
        refresh();
    }
    void tick() override {
        constexpr unsigned elapsed_per_frame = 0x8000 / 60, display_interval = 0xaaa;
        display_accumulator_ += elapsed_per_frame;
        const auto display_elapsed = display_accumulator_ / display_interval;
        display_accumulator_ %= display_interval;
        if (pending_ && menu_.activation_complete()) {
            result_ = views[std::size_t(menu_.selection().selected_id() - 401)];
            close();
            return;
        }
        if (!pending_)
            pending_ = menu_.input(std::exchange(edges_, 0), audio_.get());
        draw(int(display_elapsed));
        if (audio_)
            try {
                audio_->pump();
            } catch (const std::exception& error) {
                audio_error(error);
            }
    }
    void key(unsigned value) override {
        if (value == VK_ESCAPE) {
            close();
            return;
        }
        if (pending_)
            return;
        if (value == options_.key(0))
            edges_ |= 1;
        else if (value == options_.key(1))
            edges_ |= 2;
        else
            edges_ |= frontend_key_action(value) & 7;
    }
    std::optional<std::intptr_t> probe(unsigned id) const override {
        switch (id) {
            case 1:
                return menu_.selection().selected_id();
            case 2:
                return menu_.first_visible();
            case 4:
                return menu_.counter();
            case 5:
                return menu_.activation_remaining();
            case 8:
                return audio_ ? std::intptr_t(audio_->submitted_buffers()) : -1;
            case 14:
                return last_grid_;
            default:
                return {};
        }
    }
};
} // namespace
std::optional<unsigned> choose_camera(void* owner, const std::filesystem::path& assets, const GameOptions& options, bool audio_forced_off) {
    CameraWindow picker(assets, options, audio_forced_off);
    picker.run(owner);
    return picker.result();
}
} // namespace web
