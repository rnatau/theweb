#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include "backbox_window.hpp"
#include "../../edition.hpp"
#include "../../host_settings.hpp"
#include "frontend_window.hpp"
#include "slideshow_window.hpp"
#include "../../render/backbox_menu.hpp"
#include "../../render/frontend_menus.hpp"
#include "../../render/palette_transition.hpp"
#include "../../render/frontend_input.hpp"
#include "../../render/frontend_assets.hpp"
#include "sound_output.hpp"
#include "music_output.hpp"
#include "../diagnostic_log.hpp"
#include <iostream>
#include <utility>

namespace web {
namespace {
class BackboxWindow : public FrontendWindow {
public:
    BackboxWindow(
        const std::filesystem::path& assets,
        GameOptions& options,
        bool forced_off,
        const Edition& edition,
        BackboxEntry entry,
        HostSettings* settings,
        DiagnosticLog* reports,
        std::function<void()> prepare_play) :
        FrontendWindow(
            FrontendAssets(assets).image(entry == BackboxEntry::startup ? 1 : 5, entry == BackboxEntry::startup ? 0 : 2),
            L"TheWebOriginalBackbox",
            entry == BackboxEntry::overlay && !edition.pause_on_escape ? L"Original backbox | Flippers / arrows: choose | Enter: select | Esc: table"
                                                                       : L"Pro Pinball: The Web | Original backbox | Flippers / arrows: choose | Enter: select | Esc: quit"),
        assets_(assets),
        options_(options),
        forced_off_(forced_off),
        background_(FrontendAssets(assets).image(5, 2)),
        renderer_(assets),
        menu_(
            edition.create_menu && settings ? edition.create_menu(renderer_, FrontendAssets(assets), options, *settings, entry != BackboxEntry::startup)
                                            : std::make_unique<FrontendMenus>(renderer_, FrontendAssets(assets), options, edition.options_menu, edition.main_menu(entry != BackboxEntry::startup))),
        settings_(settings),
        edition_(edition),
        prepare_play_(std::move(prepare_play)),
        home_(entry == BackboxEntry::startup || entry == BackboxEntry::main_menu || edition.pause_on_escape),
        intro_stage_(entry == BackboxEntry::startup ? publisher_stage : menu_stage) {
        BackboxMenu::prepare(background_);
        if (intro_stage_ == publisher_stage)
            begin_intro(publisher_stage);
        if (intro_stage_ == menu_stage)
            draw(0);
        reports_ = reports ? reports : &fallback_reports_;
        diagnostics(reports_);
        configure_audio();
        gamepad(settings_ && settings_->gamepad);
    }
    bool quit() const {
        return quit_ || (home_ && !play_);
    }

private:
    DiagnosticLog fallback_reports_;
    DiagnosticLog* reports_ = &fallback_reports_;
    std::filesystem::path assets_;
    GameOptions& options_;
    bool forced_off_;
    Bitmap background_;
    DotMatrixRenderer renderer_;
    std::unique_ptr<FrontendMenus> menu_;
    HostSettings* settings_;
    const Edition& edition_;
    std::function<void()> prepare_play_;
    static constexpr unsigned publisher_stage = 0, title_stage = 1, backbox_stage = 2, menu_stage = 3;
    static constexpr unsigned publisher_frames = 120, title_frames = 180, backbox_frames = 120;
    bool home_, play_ = false;
    unsigned intro_stage_, intro_remaining_ = publisher_frames;
    PaletteTransition intro_fade_;
    bool intro_fade_out_ = false;
    Bytes intro_indices_;
    unsigned grid_ = 0, last_grid_ = 0, display_accumulator_ = 0;
    bool pending_ = false, quit_ = false;
    bool active_ = false;
    std::uint32_t edges_ = 0;
    std::unique_ptr<SoundOutput> audio_;
    std::unique_ptr<MusicOutput> music_;
    std::uint64_t music_revision_ = 1;
    void configure_audio() {
        edition_.normalize_options(options_);
        try {
            MusicCatalog catalog(assets_);
            if (intro_stage_ < backbox_stage || forced_off_ || !options_.music_enabled() || !catalog.available())
                music_.reset();
            else {
                if (!music_)
                    music_ = std::make_unique<MusicOutput>(std::move(catalog));
                music_->update({13, music_revision_, 0}, options_.music_volume());
                music_->pause(!active_);
            }
        } catch (const std::exception& error) {
            reports_->report("Menu music unavailable", error.what());
            music_.reset();
        }
        if (forced_off_ || !options_.sound_enabled() || !options_.sound_channels()) {
            audio_.reset();
            return;
        }
        try {
            if (!audio_ || audio_->rate() != options_.sound_rate() || audio_->channels() != options_.sound_channels())
                audio_ =
                    std::make_unique<SoundOutput>(assets_, options_.sound_rate(), options_.sound_channels(), SoundPcmFormat{options_.sound_stereo(), options_.sound_16bit()}, edition_.normalize_sound);
            audio_->volume(options_.volume());
            audio_->pause(!active_);
        } catch (const std::exception& error) {
            reports_->report("Menu sound unavailable", error.what());
            audio_.reset();
        }
    }
    void activity(bool active) override {
        active_ = active;
        if (music_)
            music_->pause(!active);
        if (audio_) {
            try {
                audio_->pause(!active);
            } catch (const std::exception& error) {
                reports_->report("Menu sound unavailable", error.what());
                audio_.reset();
            }
        }
    }
    void blur() override {
        edges_ = 0;
    }
    void draw(int elapsed) {
        canvas() = background_;
        last_grid_ = grid_;
        BackboxMenu::raster(canvas(), menu_->step(renderer_.grid_frame(last_grid_), elapsed));
        grid_ += unsigned(elapsed);
        if (grid_ > 3)
            grid_ = 0;
        refresh();
    }
    unsigned display_elapsed() {
        constexpr unsigned elapsed_per_frame = 0x8000 / 60, display_interval = 0xaaa;
        display_accumulator_ += elapsed_per_frame;
        const auto elapsed = display_accumulator_ / display_interval;
        display_accumulator_ %= display_interval;
        return elapsed;
    }
    void begin_intro(unsigned stage) {
        intro_stage_ = stage;
        intro_remaining_ = stage == publisher_stage ? publisher_frames : title_frames;
        const auto description = FrontendAssets::describe(1, stage);
        const auto image_path = assets_ / "FRONTEND/1" / description.filename;
        canvas() = FrontendAssets(assets_).image(1, stage);
        intro_indices_ = read_file(image_path);
        auto palette_path = image_path;
        const auto palette = read_file(palette_path.replace_extension(".PAL"));
        PaletteTransition::Palette target;
        if (palette.size() != target.size() || intro_indices_.size() != canvas().pixels.size())
            throw std::runtime_error("Invalid startup bitmap or palette");
        std::copy(palette.begin(), palette.end(), target.begin());
        std::fill(canvas().pixels.begin(), canvas().pixels.end(), 0);
        canvas().palette.fill(0);
        intro_fade_.begin(target, 64);
        intro_fade_out_ = false;
        refresh();
    }
    void draw_intro(int elapsed) {
        canvas() = background_;
        last_grid_ = grid_;
        auto frame = renderer_.grid_frame(last_grid_);
        // MENU 0x402590 uses FONT2's original lowercase title artwork.
        renderer_.text(frame, "abcdefghijklm", 2, 14, 8);
        BackboxMenu::raster(canvas(), frame);
        grid_ += unsigned(elapsed);
        if (grid_ > 3)
            grid_ = 0;
        refresh();
    }
    void begin_backbox() {
        intro_stage_ = backbox_stage;
        intro_remaining_ = backbox_frames;
        display_accumulator_ = 0;
        draw_intro(0);
        configure_audio();
    }
    void tick() override {
        if (settings_) {
            settings_->fullscreen = fullscreen();
            gamepad(settings_->gamepad || menu_->page() == 9);
        }
        if (intro_stage_ < menu_stage) {
            if (intro_fade_.active()) {
                canvas().palette = decode_palette(*intro_fade_.step());
                for (std::size_t i = 0; i < intro_indices_.size(); ++i)
                    canvas().pixels[i] = canvas().palette[intro_indices_[i]];
                refresh();
                if (!intro_fade_.active() && intro_fade_out_) {
                    if (intro_stage_ == publisher_stage)
                        begin_intro(title_stage);
                    else
                        begin_backbox();
                }
                return;
            }
            if (intro_stage_ < backbox_stage) {
                if (!--intro_remaining_) {
                    intro_fade_out_ = true;
                    intro_fade_.begin({}, 16);
                }
            } else {
                draw_intro(int(display_elapsed()));
                if (!--intro_remaining_) {
                    intro_stage_ = menu_stage;
                    draw(0);
                }
            }
            return;
        }
        // MENU 0x403789..0x4037db and 0x40253c: animation/confirmation
        // ticks are independent of the 60 Hz input/window cadence.
        const auto elapsed = display_elapsed();
        if (music_) {
            if (music_->completed())
                music_->update({13, ++music_revision_, 0}, options_.music_volume());
            if (const auto error = music_->take_error(); !error.empty())
                reports_->report("Menu music unavailable", error);
        }
        if (pending_ && menu_->activation_complete()) {
            pending_ = false;
            const auto choice = menu_->accept();
            if (choice == FrontendMenus::Action::fullscreen && settings_)
                fullscreen(settings_->fullscreen);
            if (choice == FrontendMenus::Action::play || choice == FrontendMenus::Action::quit) {
                quit_ = choice == FrontendMenus::Action::quit;
                play_ = !quit_;
                if (play_ && prepare_play_)
                    prepare_play_();
                close();
                return;
            }
            if (choice == FrontendMenus::Action::slideshow)
                show_slideshow(window(), assets_, options_);
            configure_audio();
        }
        if (!pending_)
            pending_ = menu_->input(std::exchange(edges_, 0), audio_.get());
        draw(int(elapsed));
        if (audio_) {
            try {
                audio_->pump();
                if (const auto error = audio_->take_error(); !error.empty())
                    reports_->report("Menu sound reconnecting", error);
            } catch (const std::exception& error) {
                reports_->report("Menu sound unavailable", error.what());
                audio_.reset();
            }
        }
    }
    void key(unsigned value) override {
        if (intro_stage_ < menu_stage) {
            if (intro_stage_ < backbox_stage)
                display_accumulator_ = 0;
            intro_stage_ = menu_stage;
            configure_audio();
            draw(0);
            return;
        }
        if (menu_->capture_key(value)) {
            draw(0);
            return;
        }
        if (value == VK_ESCAPE || (settings_ && value == settings_->keys[10])) {
            if (pending_)
                return;
            if (!menu_->page() || menu_->escape() == FrontendMenus::Action::quit)
                close();
            return;
        }
        if (pending_)
            return;
        if (value == (settings_ ? settings_->keys[0] : options_.key(0)))
            edges_ |= input_action::left_flipper;
        else if (value == (settings_ ? settings_->keys[1] : options_.key(1)))
            edges_ |= input_action::right_flipper;
        else if (settings_ && (value == settings_->keys[5] || value == settings_->keys[6]))
            edges_ |= input_action::launch;
        else
            edges_ |= frontend_key_action(value) & input_action::menu_navigation;
    }
    bool capture_gamepad(std::uint64_t pressed) override {
        return menu_->capture_gamepad(pressed);
    }
    std::optional<std::intptr_t> probe(unsigned id) const override {
        switch (id) {
            case 1:
                return menu_->selected_id();
            case 2:
                return menu_->first_visible();
            case 4:
                return menu_->counter();
            case 5:
                return menu_->activation_remaining();
            case 8:
                return audio_ ? std::intptr_t(audio_->submitted_buffers()) : -1;
            case 9:
                return music_ ? std::intptr_t(music_->cd_track()) : -1;
            case 10:
                return music_ ? std::intptr_t(music_->submitted_frames()) : -1;
            case 11:
                return intro_stage_;
            case 12:
                return GetConsoleWindow() ? 1 : 0;
            case 13:
                return intro_remaining_;
            case 14:
                return last_grid_;
            case 15:
                return menu_->page();
            case 16:
                return intro_stage_ < menu_stage && intro_fade_.active();
            case 17:
                return intro_fade_.remaining();
            case 18:
                return menu_->capturing();
            case 19:
                return pending_;
            default:
                return {};
        }
    }
};
} // namespace
bool show_backbox(
    void* owner,
    const std::filesystem::path& assets,
    GameOptions& options,
    bool audio_forced_off,
    const Edition& edition,
    BackboxEntry entry,
    HostSettings* settings,
    DiagnosticLog* reports,
    std::function<void()> prepare_play) {
    BackboxWindow viewer(assets, options, audio_forced_off, edition, entry, settings, reports, std::move(prepare_play));
    viewer.run(owner);
    return viewer.quit();
}
} // namespace web
