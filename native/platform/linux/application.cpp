#include "application.hpp"
#include "../diagnostic_log.hpp"
#include "smoke_scenario.hpp"
#include "../../session_lifecycle.hpp"
#include "audio.hpp"
#include "save_storage.hpp"
#include "../../configuration_store.hpp"
#include "../../edition.hpp"
#include "../../host_settings.hpp"
#include "../../score_store.hpp"
#include "../../simulation_clock.hpp"
#include "../../incremental_session.hpp"
#include "../../render/frontend_assets.hpp"
#include "../../render/frontend_input.hpp"
#include "../../render/frontend_menus.hpp"
#include "../../render/palette_transition.hpp"
#include "../../render/slideshow.hpp"
#include "../../render/table_renderer.hpp"
#include <SDL.h>
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <thread>
#include <utility>

namespace web {
namespace {
using Clock = std::chrono::steady_clock;
std::filesystem::path executable_directory() {
    // SDL resolves the executable on both Linux and macOS, regardless of CWD.
    auto* base = SDL_GetBasePath();
    if (!base)
        throw std::runtime_error(std::string("Cannot locate executable: ") + SDL_GetError());
    const std::filesystem::path directory(base);
    SDL_free(base);
    return directory;
}
std::filesystem::path assets_directory() {
    const auto directory = executable_directory();
    for (const auto& candidate : {directory / "data/PC_DAT", directory / "PC_DAT", directory.parent_path() / "data/PC_DAT"})
        if (std::filesystem::is_regular_file(candidate / "70/TABLE/VIEW.BIT") && std::filesystem::is_regular_file(candidate / "MATRIX/FONTS/FONT1.BIT"))
            return std::filesystem::canonical(candidate);
    throw std::runtime_error("Cannot find The Web assets. Place data/PC_DAT beside the executable.");
}
std::filesystem::path default_save_root() {
#ifdef __APPLE__
    if (const auto value = std::getenv("HOME"); value && *value)
        return std::filesystem::path(value) / "Library/Application Support/theweb";
    throw std::runtime_error("Cannot choose save directory: set HOME or --save-dir");
#else
    if (const auto value = std::getenv("XDG_DATA_HOME"); value && std::filesystem::path(value).is_absolute())
        return std::filesystem::path(value) / "theweb";
    if (const auto value = std::getenv("HOME"); value && *value)
        return std::filesystem::path(value) / ".local/share/theweb";
    throw std::runtime_error("Cannot choose save directory: set HOME, XDG_DATA_HOME or --save-dir");
#endif
}
unsigned key_code(SDL_Keycode key) {
    if (key >= SDLK_a && key <= SDLK_z)
        return unsigned('A' + key - SDLK_a);
    if (key >= SDLK_0 && key <= SDLK_9)
        return unsigned('0' + key - SDLK_0);
    if (key >= SDLK_F1 && key <= SDLK_F12)
        return 112 + unsigned(key - SDLK_F1);
    switch (key) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            return key_code::enter;
        case SDLK_SPACE:
            return key_code::space;
        case SDLK_ESCAPE:
            return key_code::escape;
        case SDLK_TAB:
            return key_code::tab;
        case SDLK_PAUSE:
            return key_code::pause;
        case SDLK_LEFT:
            return key_code::left;
        case SDLK_RIGHT:
            return key_code::right;
        case SDLK_UP:
            return key_code::up;
        case SDLK_DOWN:
            return key_code::down;
        case SDLK_LSHIFT:
            return key_code::left_shift;
        case SDLK_RSHIFT:
            return key_code::right_shift;
        case SDLK_LCTRL:
            return key_code::left_control;
        case SDLK_RCTRL:
            return key_code::right_control;
        case SDLK_LALT:
        case SDLK_RALT:
            return key_code::alt;
        case SDLK_KP_2:
            return key_code::numpad_2;
        case SDLK_KP_4:
            return key_code::numpad_4;
        case SDLK_KP_6:
            return key_code::numpad_6;
        case SDLK_KP_8:
            return key_code::numpad_8;
        default:
            return 0;
    }
}
class Sdl {
public:
    Sdl() {
        SDL_SetHint(SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0");
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0)
            throw std::runtime_error(SDL_GetError());
    }
    ~Sdl() {
        SDL_Quit();
    }
};
// Original MENU still owns its pages; this small native dialog substitutes for
// the Windows-only F2 dialog, using the same original font/backbox on Linux.
class ClassicOptions {
public:
    ClassicOptions(const DotMatrixRenderer& renderer, GameOptions& options) :
        renderer_(renderer),
        options_(options) {
        rebuild();
    }
    bool input(std::uint32_t edges, SoundSink* audio) {
        return list_->input(edges, audio);
    }
    bool capturing() const {
        return capture_ >= 0;
    }
    bool capture(unsigned key) {
        if (capture_ < 0)
            return false;
        if (key == key_code::escape) {
            capture_ = -1;
            return true;
        }
        try {
            options_.key(unsigned(capture_), key);
            capture_ = -1;
        } catch (const std::exception&) {}
        return true;
    }
    bool complete() const {
        return list_->activation_complete();
    }
    // Returns true to return to the table. Escape has the same save/apply path.
    bool accept() {
        const int id = list_->selection().selected_id();
        if (id == 1)
            options_.camera_selection(options_.camera_selection() % 6 + 1);
        else if (id == 2)
            options_.screen_mode(options_.screen_mode() % 6 + 1);
        else if (id == 3)
            options_.graphic_detail(options_.graphic_detail() % 3 + 1);
        else if (id == 4)
            options_.sound_enabled(!options_.sound_enabled());
        else if (id == 5)
            options_.volume(options_.volume() >= 127 ? 0 : std::min(127u, ((options_.volume() / 32) + 1) * 32));
        else if (id == 6)
            options_.music_volume(options_.music_volume() >= 255 ? 0 : std::min(255u, ((options_.music_volume() / 64) + 1) * 64));
        else if (id == 7)
            options_.visual_nudge(!options_.visual_nudge());
        else if (id >= 10 && id < 15)
            capture_ = id - 10;
        else if (id == 15)
            options_.reset_controls();
        else if (id == 16) {
            constexpr std::array rates{6000u, 11025u, 22050u, 44100u};
            const auto current = std::find(rates.begin(), rates.end(), options_.sound_rate());
            options_.sound_rate(current == rates.end() || current + 1 == rates.end() ? rates.front() : *(current + 1));
        } else if (id == 17)
            options_.sound_stereo(!options_.sound_stereo());
        else if (id == 18)
            options_.sound_16bit(!options_.sound_16bit());
        else if (id == 19)
            options_.music_restart_policy((options_.music_restart_policy() + 1) % 3);
        else if (id == 20)
            return true;
        rebuild(id);
        return false;
    }
    DotMatrixFrame step(unsigned grid, int elapsed) {
        auto frame = list_->step(renderer_.grid_frame(grid), elapsed);
        if (capture_ >= 0) {
            frame = renderer_.grid_frame(grid);
            renderer_.text(frame, "PRESS NEW KEY", 2, 0, 8);
        }
        return frame;
    }

private:
    const DotMatrixRenderer& renderer_;
    GameOptions& options_;
    std::unique_ptr<BackboxMenu> list_;
    std::vector<std::string> labels_;
    int capture_ = -1;
    void rebuild(int selected = 1) {
        labels_ = {
            "CAMERA " + std::to_string(options_.camera_selection()),
            "SCREEN MODE " + std::to_string(options_.screen_mode()),
            "DETAIL " + std::to_string(options_.graphic_detail()),
            options_.sound_enabled() ? "EFFECTS ON" : "EFFECTS OFF",
            "EFFECTS VOL " + std::to_string(options_.volume()),
            "MUSIC VOL " + std::to_string(options_.music_volume()),
            options_.visual_nudge() ? "NUDGE ON" : "NUDGE OFF"};
        std::vector<int> ids{1, 2, 3, 4, 5, 6, 7};
        for (unsigned i = 0; i < 5; ++i) {
            labels_.push_back(std::string(HostSettings::labels[i]) + " " + HostSettings::key_name(options_.key(i)));
            ids.push_back(10 + int(i));
        }
        labels_.push_back("DEFAULT KEYS");
        ids.push_back(15);
        labels_.push_back("RATE " + std::to_string(options_.sound_rate()));
        ids.push_back(16);
        labels_.push_back(options_.sound_stereo() ? "STEREO" : "MONO");
        ids.push_back(17);
        labels_.push_back(options_.sound_16bit() ? "16 BIT" : "8 BIT");
        ids.push_back(18);
        labels_.push_back("MUSIC POLICY " + std::to_string(options_.music_restart_policy() + 1));
        ids.push_back(19);
        labels_.push_back("BACK TO TABLE");
        ids.push_back(20);
        std::vector<BackboxMenu::Item> items;
        for (unsigned i = 0; i < ids.size(); ++i)
            items.push_back({ids[i], labels_[i]});
        list_ = std::make_unique<BackboxMenu>(renderer_, items);
        list_->select(selected);
    }
};
class Application {
    friend class development::SmokeScenario;

public:
    Application(LinuxLaunch launch, const Edition& edition) :
        launch_(std::move(launch)),
        edition_(edition),
        assets_(assets_directory()),
        options_(edition_.default_options()),
        frontend_(assets_),
        fonts_(assets_),
        menu_background_(frontend_.image(5, 2)),
        nudge_(edition_.create_nudge()) {
        load_saves();
        edition_.normalize_options(options_);
        if (!launch_.explicit_display)
            launch_.fullscreen = settings_.fullscreen;
        window_ = SDL_CreateWindow(
            (std::string(edition_.title) + " " + std::string(edition_.version)).c_str(),
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            1024,
            768,
            SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | (launch_.fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0));
        if (!window_)
            throw std::runtime_error(SDL_GetError());
        display_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | (edition_.incremental_presentation ? SDL_RENDERER_PRESENTVSYNC : 0));
        if (!display_)
            display_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
        if (!display_) {
            SDL_DestroyWindow(window_);
            throw std::runtime_error(SDL_GetError());
        }
        SDL_ShowCursor(SDL_DISABLE);
        open_controller();
        open_menu(false);
        begin_intro(0);
        update_audio();
    }
    ~Application() {
        menu_music_.reset();
        music_.reset();
        menu_audio_.reset();
        audio_.reset();
        if (pad_)
            SDL_GameControllerClose(pad_);
        if (texture_)
            SDL_DestroyTexture(texture_);
        SDL_DestroyRenderer(display_);
        SDL_DestroyWindow(window_);
    }
    int run() {
        previous_ = Clock::now();
        input_timeline_.reset(nanoseconds(previous_));
        unsigned smoke_tick = 0;
        auto smoke_deadline = previous_;
        while (!quit_) {
            SDL_Event event;
            while (SDL_PollEvent(&event))
                handle(event);
            if (edition_.timestamped_input && timed_play())
                poll_controller();
            auto now = Clock::now();
            if (launch_.smoke_test && now >= smoke_deadline) {
                const auto stage = smoke_.stage();
                smoke_.step(*this, smoke_tick++);
                if (stage != smoke_.stage())
                    std::cout << "SMOKE " << edition_.id << " stage " << smoke_.stage() << std::endl;
                smoke_deadline = now + std::chrono::milliseconds(17);
                if (smoke_tick > 1100)
                    throw std::runtime_error("SDL smoke test timed out at stage " + std::to_string(smoke_.stage()));
            }
            now = Clock::now();
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - previous_).count();
            previous_ = now;
            const bool active = focused_ && !minimized_;
            if (!active)
                clock_.reset();
            const auto count = active ? clock_.advance(std::uint64_t(elapsed)) : 0;
            if (edition_.incremental_presentation && timed_play()) {
                SDL_DisplayMode mode{};
                const auto index = SDL_GetWindowDisplayIndex(window_);
                const auto rate = index >= 0 && SDL_GetCurrentDisplayMode(index, &mode) == 0 && mode.refresh_rate > 1 && mode.refresh_rate <= 1000 ? unsigned(mode.refresh_rate) : 60u;
                presentation_clock_.rate(rate);
                if (presentation_clock_.advance(std::uint64_t(elapsed))) {
                    tick_live_game(nanoseconds(now));
                    update_audio();
                    present();
                }
            } else if (count) {
                if (!edition_.timestamped_input || !timed_play())
                    poll_controller();
                if (menu_ || classic_options_)
                    tick_menu(count);
                else if (slides_)
                    tick_slides(count);
                else if (!paused_)
                    tick_game(count, nanoseconds(now));
                update_audio();
                present();
                if (launch_.smoke_test && smoke_tick > 1100)
                    throw std::runtime_error("SDL smoke test timed out at stage " + std::to_string(smoke_.stage()));
            }
            const auto work = nanoseconds(Clock::now()) - nanoseconds(previous_);
            const auto remaining = edition_.incremental_presentation && timed_play() ? presentation_clock_.until_next_frame_ns() : clock_.until_next_frame_ns();
            const auto wait = std::min<std::uint64_t>(remaining > work ? remaining - work : 1, edition_.timestamped_input && timed_play() ? 1000000 : 4000000);
            std::this_thread::sleep_for(std::chrono::nanoseconds(wait));
        }
        checkpoint();
        return 0;
    }

private:
    LinuxLaunch launch_;
    SimulationClock clock_;
    IncrementalSession incremental_;
    PresentationClock presentation_clock_;
    InputTimeline input_timeline_;
    Clock::time_point previous_ = Clock::now();
    static std::uint64_t nanoseconds(Clock::time_point time) {
        return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count());
    }
    bool timed_play() const {
        return game_ && focused_ && !minimized_ && !paused_ && !menu_ && !classic_options_ && !slides_;
    }
    void record_keyboard(std::uint32_t timestamp_ms) {
        if (edition_.timestamped_input && timed_play()) {
            const auto received = nanoseconds(Clock::now());
            input_timeline_.record({input_event_time(received, SDL_GetTicks(), timestamp_ms), InputSource::keyboard, keyboard_actions() & ~host_action::commands});
        }
    }
    const Edition& edition_;
    std::filesystem::path assets_;
    GameOptions options_;
    HostSettings settings_;
    FrontendAssets frontend_;
    DiagnosticLog reports_;
    DotMatrixRenderer fonts_;
    Bitmap menu_background_;
    std::unique_ptr<ScoreStore> scores_;
    std::unique_ptr<ConfigurationStore> config_;
    std::unique_ptr<HostSettingsStore> host_;
    std::optional<Scores> records_;
    std::unique_ptr<GameSession> game_;
    std::unique_ptr<TableRenderer> table_;
    std::unique_ptr<NudgePresentation> nudge_;
    std::unique_ptr<FrontendMenus> menu_;
    std::unique_ptr<ClassicOptions> classic_options_;
    std::unique_ptr<Slideshow> slides_;
    std::unique_ptr<LinuxEffects> audio_, menu_audio_;
    std::unique_ptr<LinuxMusic> music_, menu_music_;
    GameOptions audio_options_, menu_audio_options_;
    MusicContinuation continuation_;
    std::uint64_t menu_music_revision_ = 1;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* display_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    SDL_GameController* pad_ = nullptr;
    int texture_width_ = 0, texture_height_ = 0;
    Bitmap screen_;
    std::vector<std::uint32_t> output_;
    std::array<bool, 256> keys_{}, blocked_{};
    std::uint32_t pending_ = 0, menu_edges_ = 0, previous_pad_actions_ = 0;
    std::uint64_t previous_pad_buttons_ = 0;
    bool audio_attempted_ = false, menu_audio_attempted_ = false, music_failed_ = false;
    bool menu_overlay_ = false, return_to_pause_ = false, table_flipping_ = false;
    bool pad_armed_ = false, quit_ = false, paused_ = false, focused_ = true, minimized_ = false, pending_menu_ = false;
    unsigned intro_ = 3, intro_remaining_ = 0, menu_clock_ = 0, grid_ = 0;
    Bytes intro_indices_;
    PaletteTransition intro_fade_;
    bool intro_fade_out_ = false;
    development::SmokeScenario smoke_;
    std::uint64_t nudge_presentations_ = 0;
    bool presenting_nudge_ = false;
    void load_saves() {
        if (!launch_.save)
            return;
        const auto root = launch_.save_root.empty() ? default_save_root() : launch_.save_root;
        const auto directory = std::filesystem::weakly_canonical(root / edition_.id);
        for (const auto& protected_root : {assets_, assets_.parent_path()}) {
            auto relative = directory.lexically_relative(protected_root);
            if (relative.empty() || (!relative.empty() && *relative.begin() != ".."))
                throw std::runtime_error("Save folder must be outside reference media");
        }
        reports_.directory(directory);
        scores_ = std::make_unique<ScoreStore>(directory, make_posix_save_storage());
        records_ = scores_->load();
        config_ = std::make_unique<ConfigurationStore>(directory, make_posix_save_storage());
        if (auto value = config_->load())
            options_ = *value;
        if (edition_.consolidated_menu) {
            for (unsigned i = 0; i < 5; ++i)
                settings_.keys[i] = options_.key(i);
            host_ = std::make_unique<HostSettingsStore>(directory, make_posix_save_storage());
            if (auto value = host_->load())
                settings_ = *value;
        }
        report_saves();
    }
    void report_saves() {
        for (const auto& message : {scores_ ? scores_->error() : "", config_ ? config_->error() : "", host_ ? host_->error() : ""})
            if (!message.empty())
                reports_.report("SAVING UNAVAILABLE", message);
    }
    void checkpoint() {
        if (game_ && game_->phase() != GameSession::Phase::ending)
            records_ = game_->save_scores();
        if (scores_ && records_)
            scores_->save(*records_);
        edition_.normalize_options(options_);
        if (config_)
            config_->save(options_);
        settings_.fullscreen = launch_.fullscreen;
        if (host_)
            host_->save(settings_);
        report_saves();
    }
    void neutral_input() {
        for (unsigned i = 0; i < keys_.size(); ++i)
            blocked_[i] = blocked_[i] || keys_[i];
        keys_.fill(false);
        pending_ = menu_edges_ = previous_pad_actions_ = 0;
        pad_armed_ = false;
        nudge_->reset();
        previous_ = Clock::now();
        clock_.reset();
        presentation_clock_.reset();
        if (game_ && edition_.incremental_presentation)
            incremental_.reset(nanoseconds(previous_), *game_);
        input_timeline_.reset(nanoseconds(previous_));
    }
    void toggle_fullscreen(bool value) {
        if (SDL_SetWindowFullscreen(window_, value ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0) < 0)
            throw std::runtime_error(SDL_GetError());
        launch_.fullscreen = settings_.fullscreen = value;
        SDL_RaiseWindow(window_);
        SDL_SetWindowInputFocus(window_);
    }
    void open_controller() {
        if (pad_ && SDL_GameControllerGetAttached(pad_))
            return;
        if (pad_)
            SDL_GameControllerClose(pad_);
        pad_ = nullptr;
        for (int i = 0; i < SDL_NumJoysticks(); ++i)
            if (SDL_IsGameController(i) && (pad_ = SDL_GameControllerOpen(i)))
                break;
        previous_pad_buttons_ = 0;
        pad_armed_ = false;
    }
    void poll_controller() {
        previous_pad_actions_ = 0;
        if (!pad_) {
            if (edition_.timestamped_input && timed_play())
                input_timeline_.record({nanoseconds(Clock::now()), InputSource::controller, 0});
            return;
        }
        GamepadSample sample;
        const std::array pairs = {
            std::pair{SDL_CONTROLLER_BUTTON_A,             pad_button::a    },
            std::pair{SDL_CONTROLLER_BUTTON_B,             pad_button::b    },
            std::pair{SDL_CONTROLLER_BUTTON_X,             pad_button::x    },
            std::pair{SDL_CONTROLLER_BUTTON_Y,             pad_button::y    },
            std::pair{SDL_CONTROLLER_BUTTON_LEFTSHOULDER,  pad_button::lb   },
            std::pair{SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, pad_button::rb   },
            std::pair{SDL_CONTROLLER_BUTTON_BACK,          pad_button::back },
            std::pair{SDL_CONTROLLER_BUTTON_START,         pad_button::start},
            std::pair{SDL_CONTROLLER_BUTTON_DPAD_LEFT,     pad_button::left },
            std::pair{SDL_CONTROLLER_BUTTON_DPAD_RIGHT,    pad_button::right},
            std::pair{SDL_CONTROLLER_BUTTON_DPAD_UP,       pad_button::up   },
            std::pair{SDL_CONTROLLER_BUTTON_DPAD_DOWN,     pad_button::down }
        };
        for (const auto& [button, mask] : pairs)
            if (SDL_GameControllerGetButton(pad_, button))
                sample.buttons |= mask;
        sample.x = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTX);
        sample.y = -int(SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_LEFTY));
        sample.left_trigger = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_TRIGGERLEFT) / 128;
        sample.right_trigger = SDL_GameControllerGetAxis(pad_, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) / 128;
        const auto buttons = gamepad_buttons(sample, previous_pad_buttons_);
        const auto pressed = buttons & ~std::exchange(previous_pad_buttons_, buttons);
        if (!buttons)
            pad_armed_ = true;
        if (!pad_armed_ || !edition_.consolidated_menu || (!settings_.gamepad && (!menu_ || !menu_->capturing())))
            return;
        if (menu_ && menu_->capture_gamepad(pressed))
            return;
        if (menu_ || slides_) {
            if (pressed & (pad_button::left | pad_button::up | pad_button::lb | pad_button::stick_left | pad_button::stick_up))
                menu_edges_ |= input_action::left_flipper;
            if (pressed & (pad_button::right | pad_button::down | pad_button::rb | pad_button::stick_right | pad_button::stick_down))
                menu_edges_ |= input_action::right_flipper;
            if (pressed & (pad_button::a | pad_button::start))
                menu_edges_ |= input_action::launch;
            if (pressed & (pad_button::b | pad_button::back))
                escape();
        } else {
            const auto actions = settings_.gamepad_actions(buttons);
            const auto commands = settings_.gamepad_actions(pressed);
            handle_commands(commands);
            previous_pad_actions_ = actions & ~host_action::commands;
            if (edition_.timestamped_input && timed_play())
                input_timeline_.record({nanoseconds(Clock::now()), InputSource::controller, previous_pad_actions_});
        }
    }
    std::uint32_t keyboard_actions() const {
        return edition_.consolidated_menu ? settings_.keyboard_actions(keys_) : options_.actions(keys_);
    }
    void handle_commands(std::uint32_t commands) {
        if (commands & host_action::menu)
            open_menu(false);
        else if (commands & host_action::pause) {
            paused_ = !paused_;
            neutral_input();
        } else if (commands & host_action::view)
            change_view();
    }
    void handle(const SDL_Event& event) {
        if (event.type == SDL_QUIT) {
            quit_ = true;
            return;
        }
        if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED) {
            open_controller();
            return;
        }
        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
                focused_ = false;
                neutral_input();
            }
            if (event.window.event == SDL_WINDOWEVENT_FOCUS_GAINED) {
                focused_ = true;
                neutral_input();
            }
            if (event.window.event == SDL_WINDOWEVENT_MINIMIZED) {
                minimized_ = true;
                neutral_input();
            }
            if (event.window.event == SDL_WINDOWEVENT_RESTORED)
                minimized_ = false;
            if (event.window.event == SDL_WINDOWEVENT_EXPOSED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                present();
            update_audio();
            return;
        }
        if (event.type != SDL_KEYDOWN && event.type != SDL_KEYUP)
            return;
        const auto key = key_code(event.key.keysym.sym);
        if (!key || key >= keys_.size())
            return;
        if (event.type == SDL_KEYUP) {
            keys_[key] = blocked_[key] = false;
            record_keyboard(event.key.timestamp);
            return;
        }
        if (event.key.repeat || blocked_[key])
            return;
        if (key == key_code::enter && (event.key.keysym.mod & KMOD_ALT)) {
            toggle_fullscreen(!launch_.fullscreen);
            return;
        }
        keys_[key] = true;
        if (intro_ < 3) {
            intro_ = 3;
            neutral_input();
            return;
        }
        if (classic_options_ && classic_options_->capture(key))
            return;
        if (menu_ && menu_->capture_key(key))
            return;
        if (key == key_code::escape || (menu_ && edition_.consolidated_menu && key == settings_.keys[10])) {
            escape();
            return;
        }
        if (menu_ || classic_options_ || slides_) {
            const auto left = edition_.consolidated_menu ? settings_.keys[0] : options_.key(0);
            const auto right = edition_.consolidated_menu ? settings_.keys[1] : options_.key(1);
            if (key == left)
                menu_edges_ |= input_action::left_flipper;
            else if (key == right)
                menu_edges_ |= input_action::right_flipper;
            else if (edition_.consolidated_menu && (key == settings_.keys[5] || key == settings_.keys[6]))
                menu_edges_ |= input_action::launch;
            else
                menu_edges_ |= frontend_key_action(key, bool(slides_));
            return;
        }
        if (edition_.consolidated_menu) {
            std::array<bool, 256> edge{};
            edge[key] = true;
            handle_commands(settings_.keyboard_actions(edge) & host_action::commands);
        } else {
            if (key == 'P' || key == key_code::pause) {
                paused_ = !paused_;
                neutral_input();
            } else if (key == 'V')
                change_view();
            else if (key == key_code::f2)
                open_menu(true);
            else if (key == key_code::f4)
                open_menu(false, true);
            else if (options_.adjust_volume_key(key)) {
                if (audio_)
                    audio_->volume(options_.volume());
                game_->queue_effects_volume(options_.volume());
            }
        }
        pending_ |= keyboard_actions() & ~host_action::commands;
        record_keyboard(event.key.timestamp);
    }
    void change_view() {
        options_.camera_selection(options_.camera_selection() % 6 + 1);
        reload_table();
    }
    void reload_table() {
        if (!game_)
            return;
        table_ = std::make_unique<TableRenderer>(assets_, options_.display_variant());
        table_->antialias_sprites(edition_.sprite_antialiasing);
        table_flipping_ = options_.screen_flip();
        table_->enable_lamp_timing(table_flipping_);
        table_->advance_balls(game_->table_view().motion);
        nudge_->reset();
        compose();
    }
    void compose() {
        screen_ = edition_.incremental_presentation ? table_->compose_current(game_->table_view(), options_.graphic_detail()) : table_->compose(game_->table_view(), options_.graphic_detail());
    }
    void open_menu(bool classic_options, bool overlay = false) {
        neutral_input();
        menu_overlay_ = overlay;
        return_to_pause_ = (classic_options || overlay) && paused_;
        menu_edges_ = menu_clock_ = grid_ = 0;
        pending_menu_ = false;
        intro_ = 3;
        if (classic_options)
            classic_options_ = std::make_unique<ClassicOptions>(fonts_, options_);
        else if (edition_.create_menu)
            menu_ = edition_.create_menu(fonts_, frontend_, options_, settings_, bool(game_));
        else
            menu_ = std::make_unique<FrontendMenus>(fonts_, frontend_, options_, edition_.options_menu, edition_.main_menu(bool(game_)));
        if (game_ && !classic_options && !overlay && !edition_.pause_on_escape) {
            checkpoint();
            game_->prepare_exit_display();
            compose();
            present();
            game_->sound_sink(nullptr);
            game_.reset();
            audio_attempted_ = false;
            table_.reset();
            audio_.reset();
            music_.reset();
        }
        screen_ = menu_background_;
        BackboxMenu::prepare(screen_);
        update_audio();
    }
    void escape() {
        neutral_input();
        if (slides_) {
            slides_.reset();
            open_menu(false, menu_overlay_);
        } else if (classic_options_)
            resume();
        else if (menu_) {
            if (!pending_menu_) {
                if (menu_overlay_ && !menu_->page())
                    resume();
                else if (menu_->escape() == FrontendMenus::Action::quit)
                    quit_ = true;
            }
        } else
            open_menu(false);
    }
    void resume() {
        menu_.reset();
        classic_options_.reset();
        menu_audio_.reset();
        menu_music_.reset();
        menu_audio_attempted_ = false;
        neutral_input();
        edition_.normalize_options(options_);
        if (!game_) {
            game_ = edition_.create_game(assets_);
            if (records_)
                game_->load_scores(*records_);
            game_->initialize_engine_startup();
        }
        game_->music_restart_policy(options_.music_restart_policy());
        paused_ = return_to_pause_;
        if (!table_ || table_->variant() != options_.display_variant())
            reload_table();
        else {
            if (table_flipping_ != options_.screen_flip()) {
                table_flipping_ = options_.screen_flip();
                table_->enable_lamp_timing(table_flipping_);
            }
            compose();
        }
        checkpoint();
        update_audio();
    }
    void tick_menu(unsigned count) {
        if (intro_ < 3) {
            for (unsigned tick = 0; tick < count && intro_ < 3; ++tick) {
                if (intro_fade_.active()) {
                    screen_.palette = decode_palette(*intro_fade_.step());
                    for (std::size_t i = 0; i < intro_indices_.size(); ++i)
                        screen_.pixels[i] = screen_.palette[intro_indices_[i]];
                    if (!intro_fade_.active() && intro_fade_out_) {
                        if (!intro_)
                            begin_intro(1);
                        else {
                            intro_ = 2;
                            intro_remaining_ = 120;
                            screen_ = menu_background_;
                        }
                    }
                } else if (intro_ < 2) {
                    if (!--intro_remaining_) {
                        intro_fade_out_ = true;
                        intro_fade_.begin({}, 16);
                    }
                } else {
                    menu_clock_ += 0x8000 / 60;
                    grid_ = (grid_ + menu_clock_ / 0xaaa) % 4;
                    menu_clock_ %= 0xaaa;
                    auto frame = fonts_.grid_frame(grid_);
                    fonts_.text(frame, "abcdefghijklm", 2, 14, 8);
                    screen_ = menu_background_;
                    BackboxMenu::raster(screen_, frame);
                    if (!--intro_remaining_)
                        intro_ = 3;
                }
            }
            if (intro_ < 3)
                return;
        }
        if (pending_menu_ && (menu_ ? menu_->activation_complete() : classic_options_->complete())) {
            pending_menu_ = false;
            if (classic_options_) {
                if (classic_options_->accept()) {
                    resume();
                    return;
                }
            } else {
                const auto choice = menu_->accept();
                edition_.normalize_options(options_);
                if (choice == FrontendMenus::Action::play) {
                    resume();
                    return;
                }
                if (choice == FrontendMenus::Action::quit) {
                    quit_ = true;
                    return;
                }
                if (choice == FrontendMenus::Action::fullscreen)
                    toggle_fullscreen(settings_.fullscreen);
                if (choice == FrontendMenus::Action::slideshow) {
                    menu_.reset();
                    slides_ = std::make_unique<Slideshow>();
                    menu_edges_ = 0;
                    tick_slides(1);
                    return;
                }
            }
        }
        if (!pending_menu_) {
            const auto edges = std::exchange(menu_edges_, 0u);
            pending_menu_ = menu_ ? menu_->input(edges, menu_audio_.get()) : classic_options_->input(edges, menu_audio_.get());
        }
        menu_clock_ += count * (0x8000 / 60);
        const unsigned elapsed = menu_clock_ / 0xaaa;
        menu_clock_ %= 0xaaa;
        screen_ = menu_background_;
        BackboxMenu::prepare(screen_);
        BackboxMenu::raster(screen_, menu_ ? menu_->step(fonts_.grid_frame(grid_), int(elapsed)) : classic_options_->step(grid_, int(elapsed)));
        grid_ = (grid_ + elapsed) % 4;
    }
    void begin_intro(unsigned stage) {
        intro_ = stage;
        intro_remaining_ = stage ? 180 : 120;
        intro_fade_out_ = false;
        const auto image = assets_ / "FRONTEND/1" / FrontendAssets::describe(1, stage).filename;
        screen_ = frontend_.image(1, stage);
        intro_indices_ = read_file(image);
        auto palette_path = image;
        const auto bytes = read_file(palette_path.replace_extension(".PAL"));
        PaletteTransition::Palette palette;
        if (bytes.size() != palette.size() || intro_indices_.size() != screen_.pixels.size())
            throw std::runtime_error("Invalid startup bitmap or palette");
        std::copy(bytes.begin(), bytes.end(), palette.begin());
        std::fill(screen_.pixels.begin(), screen_.pixels.end(), 0);
        intro_fade_ = PaletteTransition{};
        intro_fade_.begin(palette, 64);
    }
    void tick_slides(unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            if (const auto slide = slides_->step(std::exchange(menu_edges_, 0u)))
                screen_ = frontend_.slide(*slide);
            if (slides_->finished()) {
                slides_.reset();
                open_menu(false, menu_overlay_);
                break;
            }
        }
    }
    void tick_live_game(std::uint64_t now_ns) {
        auto previous_phase = game_->phase();
        incremental_.advance(
            now_ns,
            *game_,
            input_timeline_,
            [&] {
                nudge_->advance(1);
                game_->begin_presentation([&](std::uint32_t actions) {
                    ++nudge_presentations_;
                    if (options_.visual_nudge())
                        nudge_->trigger(screen_, options_.display_variant(), actions);
                });
                previous_phase = game_->phase();
            },
            [&] {
                table_->advance_lamps(game_->lamps(), 1, game_->lamp_animation_active(), game_->take_lamp_redraw());
                table_->advance_balls(game_->table_view().motion);
                table_->advance_raster();
                if (previous_phase != game_->phase() && game_->phase() == GameSession::Phase::game_over)
                    checkpoint();
                if (!game_->restart_ready())
                    return true;
                records_ = restart_session(game_, [&] { return edition_.create_game(assets_); }, audio_.get(), options_.music_restart_policy(), continuation_);
                neutral_input();
                return false;
            });
        pending_ = 0;
        compose();
    }
    void tick_game(unsigned count, std::uint64_t now_ns) {
        nudge_->advance(count);
        game_->begin_presentation([&](std::uint32_t actions) {
            ++nudge_presentations_;
            if (!options_.visual_nudge())
                return;
            auto retained = screen_;
            if (nudge_->trigger(screen_, options_.display_variant(), actions)) {
                presenting_nudge_ = true;
                present();
                presenting_nudge_ = false;
            }
            screen_ = std::move(retained);
        });
        for (unsigned i = 0; i < count; ++i) {
            const auto previous_phase = game_->phase();
            if (edition_.timestamped_input) {
                const auto microsteps = game_->physics_microsteps();
                game_->step_timed(input_timeline_.frame(clock_.frame_boundary_ns(now_ns, count - i), clock_.frame_boundary_ns(now_ns, count - i - 1), microsteps));
                pending_ = 0;
            } else
                game_->step((keyboard_actions() | std::exchange(pending_, 0u) | previous_pad_actions_) & ~host_action::commands);
            if (game_->restart_ready()) {
                records_ = restart_session(game_, [&] { return edition_.create_game(assets_); }, audio_.get(), options_.music_restart_policy(), continuation_);
                neutral_input();
                nudge_->reset();
                break;
            }
            if (previous_phase != game_->phase() && game_->phase() == GameSession::Phase::game_over)
                checkpoint();
        }
        table_->advance_lamps(game_->lamps(), int(count), game_->lamp_animation_active(), game_->take_lamp_redraw());
        table_->advance_balls(game_->table_view().motion);
        compose();
    }
    void configure_effects(std::unique_ptr<LinuxEffects>& output, GameOptions& previous, bool& attempted) {
        if (launch_.mute || !options_.sound_enabled() || !options_.sound_channels()) {
            output.reset();
            attempted = false;
            return;
        }
        try {
            if (!attempted || previous.sound_rate() != options_.sound_rate() || previous.sound_channels() != options_.sound_channels() || previous.sound_stereo() != options_.sound_stereo() ||
                previous.sound_16bit() != options_.sound_16bit()) {
                attempted = true;
                previous = options_;
                output = std::make_unique<LinuxEffects>(assets_, options_, edition_.normalize_sound);
                previous = options_;
            }
            if (output)
                output->volume(options_.volume());
        } catch (const std::exception& error) {
            reports_.report("SOUND UNAVAILABLE", error.what());
            output.reset();
        }
    }
    void update_audio() {
        const bool in_menu = bool(menu_) || bool(classic_options_) || bool(slides_);
        const bool inactive = !focused_ || minimized_;
        edition_.normalize_options(options_);
        if (game_) {
            game_->sound_sink(nullptr);
            configure_effects(audio_, audio_options_, audio_attempted_);
            game_->sound_sink(audio_.get());
            if (audio_)
                audio_->pause(inactive || in_menu || paused_);
        }
        if (in_menu) {
            configure_effects(menu_audio_, menu_audio_options_, menu_audio_attempted_);
            if (menu_audio_)
                menu_audio_->pause(inactive || intro_ < 2);
        }
        const bool music_enabled = !launch_.mute && options_.music_enabled() && MusicCatalog(assets_).available();
        try {
            if (!music_enabled || music_failed_) {
                music_.reset();
                menu_music_.reset();
                return;
            }
            if (game_) {
                if (!music_)
                    music_ = std::make_unique<LinuxMusic>(assets_);
                update_session_music(*game_, *music_, continuation_, options_.music_volume(), inactive || in_menu || paused_);
            }
            if (in_menu) {
                if (!menu_music_)
                    menu_music_ = std::make_unique<LinuxMusic>(assets_);
                if (menu_music_->completed())
                    ++menu_music_revision_;
                menu_music_->update({13, menu_music_revision_, 0}, options_.music_volume());
                menu_music_->pause(inactive || intro_ < 2);
            }
        } catch (const std::exception& error) {
            music_failed_ = true;
            reports_.report("MUSIC UNAVAILABLE", error.what());
            music_.reset();
            menu_music_.reset();
        }
    }
    void present() {
        if (!display_ || screen_.pixels.empty() || minimized_)
            return;
        int width = 0, height = 0;
        SDL_GetRendererOutputSize(display_, &width, &height);
        if (width <= 0 || height <= 0)
            return;
        if (!texture_ || width != texture_width_ || height != texture_height_) {
            if (texture_)
                SDL_DestroyTexture(texture_);
            texture_ = SDL_CreateTexture(display_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
            if (!texture_)
                throw std::runtime_error(SDL_GetError());
            texture_width_ = width;
            texture_height_ = height;
        }
        output_.assign(std::size_t(width) * height, 0xff000000);
        const auto viewport = presentation_viewport(screen_.width, screen_.height, width, height, PresentationScaling::fit);
        const bool table = game_ && !menu_ && !classic_options_ && !slides_;
        Bitmap artwork;
        const Bitmap* source = &screen_;
        const auto dots = table && !presenting_nudge_ ? presented_dmd(viewport, screen_.width, screen_.height, DmdRaster::layout(options_.display_variant())) : std::nullopt;
        if (dots) {
            artwork = screen_;
            const auto layout = DmdRaster::layout(options_.display_variant());
            const auto& backdrop = table_->display_backdrop();
            for (int y = 0; y < backdrop.height; ++y)
                std::copy_n(backdrop.pixels.begin() + std::size_t(y) * backdrop.width, backdrop.width, artwork.pixels.begin() + std::size_t(layout.y + y) * artwork.width + layout.x);
            source = &artwork;
        }
        for (int y = 0; y < viewport.height; ++y) {
            const auto row = std::size_t(y * source->height / viewport.height) * source->width;
            auto destination = output_.begin() + std::size_t(viewport.y + y) * width + viewport.x;
            for (int x = 0; x < viewport.width; ++x)
                destination[x] = 0xff000000 | source->pixels[row + std::size_t(x * source->width / viewport.width)];
        }
        if (dots)
            draw_presented_dmd(output_, width, height, screen_, DmdRaster::layout(options_.display_variant()), *dots);
        if (table && options_.visual_nudge()) {
            const auto offset = nudge_->offset(options_.display_variant());
            present_nudge(output_, width, height, viewport, {offset.x * viewport.width / screen_.width, offset.y * viewport.height / screen_.height});
        }
        if (const auto notice = reports_.notice(); !notice.empty()) {
            DotMatrixFrame text;
            fonts_.text(text, notice.substr(0, 25), 5, 1, 8);
            const auto image = DotMatrixRenderer::leds(text, 1);
            const int scale = width >= 320 ? 2 : 1;
            const int left = (width - image.width * scale) / 2;
            const int top = std::max(0, height - image.height * scale - 8);
            for (int y = 0; y < image.height * scale && top + y < height; ++y)
                for (int x = 0; x < image.width * scale; ++x)
                    if (left + x >= 0 && left + x < width)
                        output_[std::size_t(top + y) * width + left + x] = image.pixels[std::size_t(y / scale) * image.width + x / scale];
        }
        // draw_presented_dmd writes RGB; the streaming texture is opaque.
        SDL_SetTextureBlendMode(texture_, SDL_BLENDMODE_NONE);
        if (SDL_UpdateTexture(texture_, nullptr, output_.data(), width * int(sizeof(std::uint32_t))) < 0 || SDL_RenderCopy(display_, texture_, nullptr, nullptr) < 0)
            throw std::runtime_error(SDL_GetError());
        SDL_RenderPresent(display_);
    }
};
} // namespace
int run_linux(const LinuxLaunch& launch, const Edition& edition) {
    Sdl runtime;
    Application application(launch, edition);
    return application.run();
}
int linux_main(int argc, char** argv, bool development, const Edition& edition) {
    try {
        LinuxLaunch launch;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            const auto next = [&]() -> std::filesystem::path {
                if (++i >= argc)
                    throw std::invalid_argument("Missing value for " + arg);
                return argv[i];
            };
            if (arg == "--version") {
                std::cout << edition.title << " " << edition.version << " | " << edition.id << " | rules " << edition.rules_version << " | physics " << edition.physics_version << " | input "
                          << edition.input_version << " | SDL desktop\n";
                return 0;
            } else if (arg == "--help") {
                std::cout << edition.title << " " << edition.version
                          << "\n--windowed | --fullscreen  Choose display mode (default fullscreen).\n--mute                     Disable audio for this run.\n--save-dir PATH            Alternate "
                             "save root (edition subfolder added).\n--no-save                  Disable loading and writing saves.\n--version                  Show edition and behavioral "
                             "versions.\nEsc opens the game menu; Alt+Enter toggles fullscreen.\n";
                return 0;
            } else if (arg == "--windowed" || arg == "--fullscreen") {
                launch.fullscreen = arg == "--fullscreen";
                launch.explicit_display = true;
            } else if (arg == "--mute")
                launch.mute = true;
            else if (arg == "--no-save")
                launch.save = false;
            else if (arg == "--save-dir")
                launch.save_root = next();
            else if (development && arg == "--smoke-test")
                launch.smoke_test = true;
            else if (development && arg == "--smoke-output")
                launch.smoke_output = next();
            else
                throw std::invalid_argument("Unknown option: " + arg + ". Use --help for launch options.");
        }
        return run_linux(launch, edition);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
} // namespace web
