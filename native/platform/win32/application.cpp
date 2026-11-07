#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <dbt.h>
#include "../../web.hpp"
#include "application.hpp"
#include "inspection.hpp"
#include "../diagnostic_log.hpp"
#include "diagnostic_notice.hpp"
#include "../../session_lifecycle.hpp"
#include "presentation_buffer.hpp"
#include "scaled_artwork.hpp"
#include "table_presentation.hpp"
#include "application_paths.hpp"
#include "save_storage.hpp"
#include "../../configuration_store.hpp"
#include "../../render/nudge_feedback.hpp"
#include "../../edition.hpp"
#include "../../simulation_clock.hpp"
#include "../../incremental_session.hpp"
#include "../../render/table_renderer.hpp"
#include "../../frame_timing.hpp"
#include "../../score_store.hpp"
#include "options_dialog.hpp"
#include "backbox_window.hpp"
#include "sound_output.hpp"
#include "music_output.hpp"
#include "gamepad_input.hpp"
#include "keyboard_input.hpp"
#include "../../host_settings.hpp"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <system_error>
#include <utility>

class web::NativeApplication::Impl {
public:
    Impl(std::filesystem::path root, unsigned view, const web::Edition& selected) :
        assets(std::move(root)),
        variant(view),
        edition(selected),
        options(edition.default_options()),
        nudge(edition.create_nudge()) {}
    std::filesystem::path assets;
    unsigned variant;
    const web::Edition& edition;
    web::Bitmap background, screen;
    web::TileMap depth;
    std::map<std::string, web::Container> sprites, patches;
    std::array<int, 3> flippers{};
    std::array<bool, 3> held{};
    bool show_depth = false;
    std::unique_ptr<web::GameSession> game;
    std::unique_ptr<web::SoundOutput> audio;
    std::unique_ptr<web::MusicOutput> music;
    std::ofstream music_trace;
    std::optional<std::array<std::int64_t, 10>> music_trace_previous;
    web::MusicContinuation music_continuation;
    void trace_music(bool suspended) {
        if (!music_trace.is_open() || !game)
            return;
        const auto playback = game->music_playback();
        const auto& rules = game->rule_state();
        const auto& player = rules.players().at(game->current_player());
        const auto physical = music ? music->cd_track() : 0;
        const std::array<std::int64_t, 10> key{
            playback.track,
            std::int64_t(playback.revision),
            physical,
            game->current_player(),
            rules.live_balls,
            player.station.state,
            player.station.captures,
            rules.video_active,
            rules.mission_time != 0,
            suspended};
        if (music_trace_previous == key)
            return;
        music_trace_previous = key;
        music_trace << nanoseconds(std::chrono::steady_clock::now()) << ',' << game->frames();
        for (const auto value : key)
            music_trace << ',' << value;
        music_trace << ',' << int(rules.phase()) << ',' << rules.mission_time << '\n';
        music_trace.flush();
    }
    void configure_music() {
        web::MusicCatalog catalog(assets);
        if (diagnostics.mute || !options.music_enabled() || !catalog.available()) {
            music.reset();
            return;
        }
        if (!music)
            music = std::make_unique<web::MusicOutput>(std::move(catalog));
        music->update(music_continuation.playback(game->music_playback()), options.music_volume());
        music->pause(paused || !focused || sizing);
    }
    void update_music(bool suspended) {
        if (!music) {
            trace_music(suspended);
            return;
        }
        web::update_session_music(*game, *music, music_continuation, options.music_volume(), suspended);
        if (const auto error = music->take_error(); !error.empty())
            reports.report("Music unavailable", error);
        trace_music(suspended);
    }
    web::TablePresentation table_presentation;
    bool gpu_presented = false;
    web::DiagnosticLog reports;
    web::ApplicationPersistence persistence;
    std::unique_ptr<web::ScoreStore> score_store;
    std::optional<web::Scores> stable_scores;
    web::GameOptions options;
    std::unique_ptr<web::ConfigurationStore> configuration_store;
    web::HostSettings host_settings;
    std::unique_ptr<web::HostSettingsStore> host_store;
    std::unique_ptr<web::GamepadInput> gamepad;
    std::uint32_t pad_actions = 0;
    bool pad_armed = false, pad_connected = false;
    bool reported_options_error = false;
    std::array<bool, 256> keys{};
    std::array<bool, 256> restart_held_keys{};
    web::windows::RawKeyboardState raw_keyboard;
    bool raw_keyboard_registered = false;
    unsigned last_key = 0;
    std::uint64_t nudge_presentations = 0;
    std::unique_ptr<web::NudgePresentation> nudge;
    bool presenting_nudge = false;
    bool exit_display_prepared = false;
    void present_exit_display(HWND window) {
        if (!game || exit_display_prepared)
            return;
        exit_display_prepared = true;
        game->prepare_exit_display();
        renderer->draw_display(screen, game->dot_matrix_frame(), false);
        InvalidateRect(window, nullptr, FALSE);
        UpdateWindow(window);
    }
    bool reported_save_error = false;
    void report_save_error() {
        if (!score_store->error().empty() && !reported_save_error) {
            reports.report("Scores not saved", score_store->error());
            reported_save_error = true;
        }
    }
    void checkpoint_scores() {
        // Phase five may have inserted a score whose initials are unconfirmed.
        if (!game || game->phase() == web::GameSession::Phase::ending)
            return;
        stable_scores = game->save_scores();
        if (!score_store)
            return;
        score_store->save(*stable_scores);
        report_save_error();
    }
    void checkpoint_options() {
        edition.normalize_options(options);
        if (host_store) {
            host_settings.fullscreen = display.fullscreen;
            host_store->save(host_settings);
            if (!host_store->error().empty() && !reported_options_error) {
                reports.report("Controls not saved", host_store->error());
                reported_options_error = true;
            }
        }
        if (!configuration_store)
            return;
        configuration_store->save(options);
        report_options_error();
    }
    void report_options_error() {
        if (!configuration_store->error().empty() && !reported_options_error) {
            reports.report("Options not saved", configuration_store->error());
            reported_options_error = true;
        }
    }
    void load_persistence() {
        if (!persistence.enabled)
            return;
        auto directory = persistence.directory.empty() ? (web::executable_directory() / "saves") : persistence.directory;
        if (persistence.edition_subdirectory)
            directory /= edition.id;
        const auto candidate = std::filesystem::weakly_canonical(directory);
        // Never place generated saves in either the reference media or an asset root.
        for (const auto& protected_root : {std::filesystem::path("data"), assets}) {
            const auto root = std::filesystem::weakly_canonical(protected_root);
            auto part = candidate.begin();
            auto protected_part = root.begin();
            while (part != candidate.end() && protected_part != root.end() && CompareStringOrdinal(part->c_str(), -1, protected_part->c_str(), -1, TRUE) == CSTR_EQUAL) {
                ++part;
                ++protected_part;
            }
            if (protected_part == root.end())
                throw std::runtime_error("Save folder must be outside the reference media and asset root");
        }
        reports.directory(candidate);
        score_store = std::make_unique<web::ScoreStore>(candidate, web::make_win32_save_storage());
        if (const auto scores = score_store->load())
            game->load_scores(*scores);
        report_save_error();
        configuration_store = std::make_unique<web::ConfigurationStore>(candidate, web::make_win32_save_storage());
        if (const auto loaded = configuration_store->load())
            options = *loaded;
        report_options_error();
        if (edition.consolidated_menu) {
            for (unsigned i = 0; i < 5; ++i)
                host_settings.keys[i] = options.key(i);
            host_store = std::make_unique<web::HostSettingsStore>(candidate, web::make_win32_save_storage());
            if (const auto loaded = host_store->load())
                host_settings = *loaded;
            if (!display.explicit_mode)
                display.fullscreen = host_settings.fullscreen;
            host_settings.fullscreen = display.fullscreen;
            if (!host_store->error().empty())
                reports.report("Controls not loaded", host_store->error());
        }
    }
    void configure_audio() {
        edition.normalize_options(options);
        game->music_restart_policy(options.music_restart_policy());
        try {
            configure_music();
        } catch (const std::exception& error) {
            music.reset();
            reports.report("Music unavailable", error.what());
        }
        game->sound_sink(nullptr);
        if (diagnostics.mute || !options.sound_enabled() || !options.sound_channels()) {
            audio.reset();
            return;
        }
        try {
            if (!audio || audio->rate() != options.sound_rate() || audio->channels() != options.sound_channels() || audio->format().stereo != options.sound_stereo() ||
                audio->format().sixteen_bit != options.sound_16bit())
                audio = std::make_unique<web::SoundOutput>(
                    assets,
                    options.sound_rate(),
                    options.sound_channels(),
                    web::SoundPcmFormat{options.sound_stereo(), options.sound_16bit()},
                    edition.normalize_sound);
            audio->volume(options.volume());
            game->sound_sink(audio.get());
        } catch (const std::exception& error) {
            audio.reset();
            reports.report("Sound unavailable", error.what());
        }
    }
    void apply_options(HWND window, unsigned old_volume) {
        edition.normalize_options(options);
        configure_audio();
        if (options.volume() != old_volume)
            game->show_effects_volume(options.volume());
        const auto new_variant = options.display_variant();
        if (variant != new_variant) {
            variant = new_variant;
            load_view();
            compose();
            fit_pixel_window(window);
        }
        checkpoint_options();
    }
    void restore_after_frontend(HWND window, bool was_paused) {
        paused = was_paused;
        keys.fill(false);
        last_key = 0;
        held.fill(false);
        actions = pending_actions = 0;
        reset_pacing();
        compose();
        title(window);
        InvalidateRect(window, nullptr, FALSE);
    }
    void show_options(HWND window) {
        const bool was_paused = paused;
        const auto old_volume = options.volume();
        paused = true;
        keys.fill(false);
        held.fill(false);
        actions = pending_actions = 0;
        if (audio)
            audio->pause(true);
        if (music)
            music->pause(true);
        reset_pacing();
        title(window);
        auto draft_display = display;
        if (web::edit_options(window, options, diagnostics.mute, assets, draft_display, edition)) {
            display.scaling = draft_display.scaling;
            apply_options(window, old_volume);
            set_fullscreen(window, draft_display.fullscreen);
        }
        restore_after_frontend(window, was_paused);
    }
    void show_backbox(HWND window, web::BackboxEntry entry = web::BackboxEntry::overlay) {
        const bool was_paused = paused;
        const auto previous = options.configuration();
        const auto old_volume = options.volume();
        paused = true;
        keys.fill(false);
        held.fill(false);
        actions = pending_actions = 0;
        if (audio)
            audio->pause(true);
        if (music)
            music->pause(true);
        reset_pacing();
        title(window);
        const bool home = entry != web::BackboxEntry::overlay;
        const bool restarting_engine = entry == web::BackboxEntry::main_menu;
        if (restarting_engine) {
            // Original WGAME exits and launches MENU Q. PLAY launches a fresh
            // WGAME process; only confirmed persistent records cross that edge.
            present_exit_display(window);
            checkpoint_scores();
            checkpoint_options();
            game->sound_sink(nullptr);
            audio.reset();
            music.reset();
        }
        // Keep the retained table underneath its owned pause menu. It covers
        // the desktop while the menu loads/paints and when RESUME closes it.
        if (home && entry != web::BackboxEntry::paused_session)
            ShowWindow(window, SW_HIDE);
        bool prepared = false;
        const auto prepare_play = [&] {
            if (restarting_engine) {
                game = edition.create_game(assets);
                exit_display_prepared = false;
                game->initialize_engine_startup();
                if (stable_scores)
                    game->load_scores(*stable_scores);
                paused = false;
                configure_audio();
                load_view();
            }
            if (options.configuration() != previous)
                apply_options(window, old_volume);
            compose();
            // The owned menu stays above the table until its first complete
            // paint. Showing the disabled owner must not activate it or remove
            // the menu before assets, audio and the presentation device are ready.
            if (home)
                ShowWindow(window, SW_SHOWNOACTIVATE);
            InvalidateRect(window, nullptr, FALSE);
            UpdateWindow(window);
            prepared = true;
        };
        const bool quit = web::show_backbox(window, assets, options, diagnostics.mute, edition, entry, edition.consolidated_menu ? &host_settings : nullptr, &reports, prepare_play);
        if (!prepared && options.configuration() != previous)
            apply_options(window, old_volume);
        if (edition.consolidated_menu)
            checkpoint_options();
        pad_actions = 0;
        pad_armed = false;
        if (home && !quit) {
            ShowWindow(window, SW_SHOW);
            SetActiveWindow(window);
            SetFocus(window);
        }
        restore_after_frontend(window, restarting_engine || entry == web::BackboxEntry::paused_session ? false : was_paused);
        if (quit) {
            if (!home)
                present_exit_display(window);
            DestroyWindow(window);
        }
    }
    std::unique_ptr<web::TableRenderer> renderer;
    web::SimulationClock simulation_clock;
    web::IncrementalSession incremental;
    web::PresentationClock presentation_clock;
    HMONITOR presentation_monitor = nullptr;
    web::GameSession::Phase tick_previous_phase = web::GameSession::Phase::ready;
    void refresh_presentation_rate(HWND window) {
        if (!edition.incremental_presentation)
            return;
        const auto monitor = MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        if (monitor == presentation_monitor)
            return;
        presentation_monitor = monitor;
        unsigned rate = 60;
        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);
        DEVMODEW mode{};
        mode.dmSize = sizeof(mode);
        if (GetMonitorInfoW(monitor, reinterpret_cast<MONITORINFO*>(&info)) && EnumDisplaySettingsExW(info.szDevice, ENUM_CURRENT_SETTINGS, &mode, 0) && mode.dmDisplayFrequency > 1 &&
            mode.dmDisplayFrequency <= 1000)
            rate = unsigned(mode.dmDisplayFrequency);
        presentation_clock.rate(diagnostics.presentation_rate ? diagnostics.presentation_rate : rate);
    }
    void begin_live_tick() {
        nudge->advance(1);
        game->begin_presentation([&](std::uint32_t actions) {
            ++nudge_presentations;
            if (options.visual_nudge())
                nudge->trigger(screen, variant, actions);
        });
        tick_previous_phase = game->phase();
    }
    bool finish_live_tick() {
        input_active_microsteps = incremental.active_microsteps();
        ++timed_input_frames;
        renderer->advance_lamps(game->lamps(), 1, game->lamp_animation_active(), game->take_lamp_redraw());
        renderer->advance_balls(game->table_view().motion);
        renderer->advance_raster();
        if (tick_previous_phase != game->phase() && game->phase() == web::GameSession::Phase::game_over)
            checkpoint_scores();
        if (!game->restart_ready())
            return true;
        web::restart_session(game, [&] { return edition.create_game(assets); }, audio.get(), options.music_restart_policy(), music_continuation);
        exit_display_prepared = false;
        restart_held_keys = keys;
        keys.fill(false);
        held.fill(false);
        actions = pending_actions = last_key = pad_actions = 0;
        pad_armed = false;
        reset_pacing();
        return false;
    }
    web::InputTimeline input_timeline;
    std::optional<std::uint64_t> dispatched_input_time;
    std::uint64_t timed_input_frames = 0;
    std::array<unsigned, 32> input_active_microsteps{};
    std::chrono::steady_clock::time_point last_tick;
    std::uint32_t actions = 0;
    std::uint32_t pending_actions = 0;
    bool paused = false, focused = true;
    bool sizing = false;
    web::ApplicationDiagnostics diagnostics;
    web::FrameTiming timing;
    web::WindowDisplay display;
    RECT windowed_rectangle{};
    bool retained_window = false;
    std::vector<web::FrameMeasurement> frame_log;
    std::chrono::steady_clock::time_point frame_work_start;
    unsigned frame_steps = 0;
    bool frame_pending = false;
    bool high_resolution_timer = false;
    static constexpr LPARAM alt_pressed_bit = LPARAM(1) << 29, previous_key_down_bit = LPARAM(1) << 30;
    static constexpr DWORD window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    int pixel_scale = 1, window_width = 0, window_height = 0;
    bool fitting_window = false;
    HMONITOR display_monitor = nullptr;
    void fit_pixel_window(HWND window, HMONITOR monitor = nullptr, UINT dpi = 0) {
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        display_monitor = monitor ? monitor : MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        if (!GetMonitorInfoW(display_monitor, &info))
            throw std::runtime_error("Cannot query monitor work area");
        if (!dpi)
            dpi = GetDpiForWindow(window);
        const auto native_height = screen.height;
        if (display.fullscreen) {
            window_width = info.rcMonitor.right - info.rcMonitor.left;
            window_height = info.rcMonitor.bottom - info.rcMonitor.top;
            pixel_scale = std::max(1, std::min(window_width / screen.width, window_height / screen.height));
            fitting_window = true;
            const auto positioned = SetWindowPos(window, nullptr, info.rcMonitor.left, info.rcMonitor.top, window_width, window_height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            fitting_window = false;
            if (!positioned)
                throw std::runtime_error("Cannot fit fullscreen window");
            InvalidateRect(window, nullptr, FALSE);
            return;
        }
        RECT frame{};
        if (!AdjustWindowRectExForDpi(&frame, window_style, FALSE, 0, dpi))
            throw std::runtime_error("Cannot calculate pixel-exact window frame");
        const int border_width = frame.right - frame.left, border_height = frame.bottom - frame.top;
        const int available_width = info.rcWork.right - info.rcWork.left, available_height = info.rcWork.bottom - info.rcWork.top;
        pixel_scale = screen.width * 2 + border_width <= available_width && native_height * 2 + border_height <= available_height ? 2 : 1;
        window_width = screen.width * pixel_scale + border_width;
        window_height = native_height * pixel_scale + border_height;
        const int x = info.rcWork.left + std::max(0, (available_width - window_width) / 2);
        const int y = info.rcWork.top + std::max(0, (available_height - window_height) / 2);
        fitting_window = true;
        const auto positioned = SetWindowPos(window, nullptr, x, y, window_width, window_height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        fitting_window = false;
        if (!positioned)
            throw std::runtime_error("Cannot set native pixel window size");
        InvalidateRect(window, nullptr, FALSE);
    }
    void set_fullscreen(HWND window, bool enabled) {
        if (display.fullscreen == enabled)
            return;
        if (enabled) {
            if (!GetWindowRect(window, &windowed_rectangle))
                throw std::runtime_error("Cannot retain window position");
            retained_window = true;
        }
        display.fullscreen = enabled;
        host_settings.fullscreen = enabled;
        const auto retained_style = GetWindowLongPtrW(window, GWL_STYLE) & (WS_VISIBLE | WS_DISABLED | WS_MINIMIZE);
        fitting_window = true;
        SetLastError(0);
        const auto previous = SetWindowLongPtrW(window, GWL_STYLE, (enabled ? WS_POPUP : window_style) | retained_style);
        const auto error = GetLastError();
        fitting_window = false;
        if (!previous && error)
            throw std::system_error(int(error), std::system_category(), "Cannot change fullscreen style");
        const auto restore_monitor = !enabled && retained_window ? MonitorFromRect(&windowed_rectangle, MONITOR_DEFAULTTONEAREST) : nullptr;
        fit_pixel_window(window, restore_monitor);
        if (!enabled && retained_window && windowed_rectangle.right - windowed_rectangle.left == window_width && windowed_rectangle.bottom - windowed_rectangle.top == window_height) {
            MONITORINFO info{};
            info.cbSize = sizeof(info);
            if (GetMonitorInfoW(display_monitor, &info) && windowed_rectangle.left >= info.rcWork.left && windowed_rectangle.top >= info.rcWork.top && windowed_rectangle.right <= info.rcWork.right &&
                windowed_rectangle.bottom <= info.rcWork.bottom) {
                fitting_window = true;
                SetWindowPos(window, nullptr, windowed_rectangle.left, windowed_rectangle.top, window_width, window_height, SWP_NOZORDER | SWP_NOACTIVATE);
                fitting_window = false;
            }
        }
        keys.fill(false);
        held.fill(false);
        actions = pending_actions = 0;
        last_key = 0;
        reset_pacing();
        title(window);
        if (IsWindowVisible(window) && IsWindowEnabled(window))
            SetFocus(window);
    }
    static std::uint64_t nanoseconds(std::chrono::steady_clock::time_point now) {
        return std::uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count());
    }
    void reset_pacing() {
        nudge->reset();
        simulation_clock.reset();
        presentation_clock.reset();
        timing.suspend();
        frame_pending = false;
        last_tick = std::chrono::steady_clock::now();
        if (game && edition.incremental_presentation)
            incremental.reset(nanoseconds(last_tick), *game, (actions | pad_actions) & ~web::host_action::commands);
        input_timeline.reset(nanoseconds(last_tick), actions, pad_armed ? pad_actions & ~web::host_action::commands : 0);
    }
    bool running(HWND window) const {
        return !paused && focused && !sizing && !IsIconic(window);
    }
    void change_view(HWND window) {
        constexpr std::array<unsigned, 6> views = {0, 1, 2, 4, 5, 6};
        auto index = std::find(views.begin(), views.end(), variant & 15) - views.begin();
        variant = (variant & 0xf0) | views[(index + 1) % views.size()];
        if (game) {
            options.view(variant & 15);
            checkpoint_options();
        }
        load_view();
        compose();
        fit_pixel_window(window);
        reset_pacing();
        title(window);
        InvalidateRect(window, nullptr, FALSE);
    }
    bool poll_gamepad(HWND window) {
        if (!gamepad || !focused || sizing || IsIconic(window)) {
            pad_actions = 0;
            pad_armed = false;
            return false;
        }
        const auto reading = gamepad->poll(window);
        if (reading.connected && !pad_connected)
            pad_armed = false;
        pad_connected = reading.connected;
        if (!reading.buttons)
            pad_armed = true;
        const auto next = pad_armed ? host_settings.gamepad_actions(reading.buttons) : 0;
        const auto edges = next & ~pad_actions;
        pad_actions = next;
        if (edition.timestamped_input && running(window))
            input_timeline.record({nanoseconds(std::chrono::steady_clock::now()), web::InputSource::controller, next & ~web::host_action::commands});
        if (edges & web::host_action::menu) {
            show_backbox(window, web::BackboxEntry::paused_session);
            return true;
        }
        if (edges & web::host_action::pause) {
            paused = !paused;
            reset_pacing();
            title(window);
        }
        if (edges & web::host_action::view)
            change_view(window);
        return false;
    }
    void game_frame(HWND window) {
        if (poll_gamepad(window))
            return;
        update_music(!running(window));
        if (audio) {
            try {
                audio->pause(!running(window));
            } catch (const std::exception& error) {
                reports.report("Sound unavailable", error.what());
                game->sound_sink(nullptr);
                audio.reset();
            }
        }
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last_tick).count();
        last_tick = now;
        if (!running(window)) {
            simulation_clock.reset();
            timing.suspend();
            frame_pending = false;
            return;
        }
        unsigned count = 0;
        if (edition.incremental_presentation) {
            refresh_presentation_rate(window);
            const bool deadline = presentation_clock.advance(std::uint64_t(elapsed));
            const bool gpu_clock = table_presentation.gpu_active() && IsWindowVisible(window) && !diagnostics.presentation_rate;
            if (gpu_clock ? !table_presentation.acquire_frame(reports) : !deadline)
                return;
            const auto work = incremental.advance(nanoseconds(now), *game, input_timeline, [&] { begin_live_tick(); }, [&] { return finish_live_tick(); });
            count = work.ticks;
            pending_actions = last_key = 0;
        } else {
            count = simulation_clock.advance(std::uint64_t(elapsed));
            if (!count)
                return;
        }
        if (!edition.incremental_presentation) {
            nudge->advance(count);
            game->begin_presentation([&](std::uint32_t nudge_actions) {
                if (!options.visual_nudge())
                    return;
                auto retained = screen;
                if (nudge->trigger(screen, variant, nudge_actions)) {
                    presenting_nudge = true;
                    InvalidateRect(window, nullptr, FALSE);
                    UpdateWindow(window);
                    presenting_nudge = false;
                    screen = std::move(retained);
                }
                ++nudge_presentations;
            });
            frame_work_start = now;
            frame_steps = count;
            frame_pending = true;
            for (unsigned i = 0; i < count; i++) {
                const auto key = std::exchange(last_key, 0u);
                if (!edition.consolidated_menu && options.adjust_volume_key(key)) {
                    if (audio)
                        audio->volume(options.volume());
                    game->queue_effects_volume(options.volume());
                }
                const auto previous_phase = game->phase();
                if (edition.timestamped_input) {
                    const auto start = simulation_clock.frame_boundary_ns(nanoseconds(now), count - i);
                    const auto end = simulation_clock.frame_boundary_ns(nanoseconds(now), count - i - 1);
                    const auto microsteps = game->physics_microsteps();
                    const auto input = input_timeline.frame(start, end, microsteps);
                    input_active_microsteps.fill(0);
                    for (const auto& sample : input.microsteps)
                        for (unsigned bit = 0; bit < input_active_microsteps.size(); ++bit)
                            input_active_microsteps[bit] += bool(sample.actions & (1u << bit));
                    ++timed_input_frames;
                    game->step_timed(input);
                } else
                    game->step((actions | pending_actions | pad_actions) & ~web::host_action::commands);
                if (game->restart_ready()) {
                    web::restart_session(game, [&] { return edition.create_game(assets); }, audio.get(), options.music_restart_policy(), music_continuation);
                    exit_display_prepared = false;
                    restart_held_keys = keys;
                    keys.fill(false);
                    held.fill(false);
                    actions = pending_actions = last_key = 0;
                    pad_actions = 0;
                    pad_armed = false;
                    nudge->reset();
                    reset_pacing();
                    break;
                }
                if (previous_phase != game->phase() && game->phase() == web::GameSession::Phase::game_over)
                    checkpoint_scores();
                pending_actions = 0;
            }
            renderer->advance_lamps(game->lamps(), std::int32_t(count), game->lamp_animation_active(), game->take_lamp_redraw());
            renderer->advance_balls(game->table_view().motion);
        }
        frame_work_start = now;
        frame_steps = count;
        frame_pending = true;
        compose();
        if (audio) {
            try {
                audio->pump();
                if (const auto error = audio->take_error(); !error.empty())
                    reports.report("Sound device reconnecting", error);
            } catch (const std::exception& error) {
                reports.report("Sound unavailable", error.what());
                game->sound_sink(nullptr);
                audio.reset();
            }
        }
        title(window);
        InvalidateRect(window, nullptr, FALSE);
        update_music(false);
        // Complete the owned paint before the next deadline. Hidden diagnostic
        // captures request the same paint path as visible UpdateWindow calls.
        if (!diagnostics.frame_log.empty() && !IsWindowVisible(window))
            SendMessageW(window, WM_PAINT, 0, 0);
        else
            UpdateWindow(window);
    }
    int game_loop(HWND window) {
        struct Timer {
            HANDLE handle = nullptr;
            ~Timer() {
                if (handle)
                    CloseHandle(handle);
            }
        } timer;
        timer.handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
        high_resolution_timer = timer.handle != nullptr;
        if (!timer.handle)
            timer.handle = CreateWaitableTimerW(nullptr, FALSE, nullptr);
        if (!timer.handle)
            throw std::runtime_error("Cannot create game frame deadline timer");
        for (;;) {
            MSG message{};
            unsigned handled = 0;
            while (handled++ < 64 && PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT)
                    return int(message.wParam);
                TranslateMessage(&message);
                if (edition.timestamped_input && (web::windows::keyboard_message(message.message) || message.message == WM_INPUT))
                    dispatched_input_time = web::input_event_time(nanoseconds(std::chrono::steady_clock::now()), DWORD(GetTickCount64()), message.time);
                DispatchMessageW(&message);
                dispatched_input_time.reset();
            }
            if (!raw_keyboard_registered && game && focused && GetFocus() == window)
                web::windows::release_missing_modifiers(
                    keys,
                    restart_held_keys,
                    [](unsigned key) { return (GetKeyState(int(key)) & 0x8000) != 0; },
                    [&](unsigned key) { update_keyboard_key(window, key, false); });
            game_frame(window);
            DWORD result;
            if (running(window)) {
                const auto work = nanoseconds(std::chrono::steady_clock::now()) - nanoseconds(last_tick);
                const auto remaining = edition.incremental_presentation ? presentation_clock.until_next_frame_ns() : simulation_clock.until_next_frame_ns();
                const auto ready = edition.incremental_presentation && IsWindowVisible(window) && !diagnostics.presentation_rate ? table_presentation.frame_ready_handle() : nullptr;
                // Sample controller state between presentations as well. This
                // is a best-effort 1 ms wake, not a device polling-rate promise.
                const auto deadline_delay = ready ? 1000000 : remaining > work ? remaining - work : 1;
                const auto delay = edition.timestamped_input ? std::min<std::uint64_t>(deadline_delay, 1000000) : deadline_delay;
                LARGE_INTEGER due;
                due.QuadPart = -LONGLONG((delay + 99) / 100);
                if (!SetWaitableTimer(timer.handle, &due, 0, nullptr, nullptr, FALSE))
                    throw std::runtime_error("Cannot arm game frame deadline timer");
                const HANDLE handles[]{timer.handle, ready};
                result = MsgWaitForMultipleObjectsEx(ready ? 2 : 1, handles, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
                if (ready && result == WAIT_OBJECT_0 + 1)
                    table_presentation.frame_ready();
            } else {
                CancelWaitableTimer(timer.handle);
                result = MsgWaitForMultipleObjectsEx(0, nullptr, INFINITE, QS_ALLINPUT, MWMO_INPUTAVAILABLE);
            }
            if (result == WAIT_FAILED)
                throw std::runtime_error("Cannot wait for game input/frame deadline");
        }
    }
    void save_frame_log() const {
        if (diagnostics.frame_log.empty())
            return;
        std::ofstream file(diagnostics.frame_log);
        if (!file)
            throw std::runtime_error("Cannot write frame timing log");
        file << "frame,game_frame,timestamp_ns,interval_ns,work_ns,simulation_steps\n";
        for (const auto& row : frame_log)
            file << row.frame << ',' << row.game_frame << ',' << row.timestamp_ns << ',' << row.interval_ns << ',' << row.work_ns << ',' << row.simulation_steps << '\n';
        if (!file)
            throw std::runtime_error("Frame timing log write failed");
    }
    void debug_overlay(HDC dc, int width, int top) const {
        if (!game || !diagnostics.overlay)
            return;
        const auto stats = timing.statistics();
        char text[256];
        std::snprintf(
            text,
            sizeof(text),
            "FRAME %llu   GAME %llu   FPS %.1f / %u\n%.2f ms   p95 %.2f   p99 %.2f   max %.2f\n%s   Jitter %.2f ms   >25ms %zu/%zu   F3: hide",
            static_cast<unsigned long long>(timing.frames() + (frame_pending ? 1 : 0)),
            static_cast<unsigned long long>(game->frames()),
            stats.fps,
            edition.incremental_presentation ? presentation_clock.rate() : 60u,
            stats.mean_ms,
            stats.p95_ms,
            stats.p99_ms,
            stats.max_ms,
            table_presentation.gpu_active() ? "DirectX" : "GDI",
            stats.stddev_ms,
            stats.over_25ms,
            stats.samples);
        const int saved = SaveDC(dc);
        SelectObject(dc, GetStockObject(ANSI_FIXED_FONT));
        RECT box{8, top + 8, std::min(width - 8, 472), top + 64};
        FillRect(dc, &box, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        SetTextColor(dc, RGB(240, 245, 250));
        SetBkMode(dc, TRANSPARENT);
        box.left += 6;
        box.top += 4;
        DrawTextA(dc, text, -1, &box, DT_LEFT | DT_TOP | DT_NOPREFIX);
        RestoreDC(dc, saved);
    }
    void load_view() {
        if (game) {
            renderer = std::make_unique<web::TableRenderer>(assets, variant);
            renderer->enable_lamp_timing();
            return;
        }
        background = web::load_table(assets, variant);
        screen = background;
        auto directory = assets / web::variant_name(variant);
        depth = web::TileMap::load(directory / "TABLE/VIEW.MAP", background.width / 8);
        for (auto name : {"FLIPL", "FLIPR", "FLIPU", "SPINNERA", "SPINNERB"})
            sprites[name] = web::Container::load(directory / "SPRITES" / (std::string(name) + ".SPR"));
        for (auto name : {"FLIPL", "FLIPR", "FLIPU", "SPINA", "SPINB"})
            patches[name] = web::Container::load(directory / "TABLE" / (std::string(name) + ".ZMP"), 16);
        flippers.fill(0);
        held.fill(false);
    }
    void compose(bool with_sprites = true) {
        if (game) {
            screen = edition.incremental_presentation ? renderer->compose_current(game->table_view(), options.graphic_detail(), show_depth)
                                                      : renderer->compose(game->table_view(), options.graphic_detail(), show_depth);
            return;
        }
        screen = background;
        const int depth_top = web::table_depth_top(variant);
        const int bpp = web::display_mode(variant).bytes_per_pixel;
        for (int i = 0; i < 3; i++)
            depth.patch(patches.at(std::array{"FLIPL", "FLIPR", "FLIPU"}[i]), flippers[i]);
        if (show_depth) {
            for (int y = 0; y < std::min(depth.rows * 8, screen.height - depth_top); y++)
                for (int x = 0; x < screen.width; x++) {
                    auto v = std::uint32_t(depth.sample(x, y));
                    screen.pixels[std::size_t(y + depth_top) * screen.width + x] = v * 0x010101;
                }
        } else if (with_sprites) {
            for (int i = 0; i < 3; i++)
                web::draw_sprite(screen, sprites.at(std::array{"FLIPL", "FLIPR", "FLIPU"}[i]), flippers[i], bpp, 0, 0);
            web::draw_sprite(screen, sprites.at("SPINNERA"), 0, bpp, 0, 0);
            web::draw_sprite(screen, sprites.at("SPINNERB"), 0, bpp, 0, 0);
        }
    }
    void title(HWND window) {
        if (game) {
            const auto text = std::string(edition.title) + " " + std::string(edition.version) + " | game session | " +
                              (paused ? std::string("PAUSED") : (!focused ? std::string("INACTIVE") : game->status())) + " | Score " + game->score().text() +
                              (score_store && !score_store->error().empty() ? " | SCORES NOT SAVED" : "") +
                              (configuration_store && !configuration_store->error().empty() ? " | OPTIONS NOT SAVED" : "") +
                              (edition.consolidated_menu ? " | Esc: menu, Alt+Enter: fullscreen" : " | Enter: launch, P: pause, V: view, F2: options, F4: backbox, Alt+Enter: fullscreen");
            SetWindowTextA(window, text.c_str());
            return;
        }
        const auto text = "The Web | native asset preview " + web::variant_name(variant) + " | F/J/U: flippers, V: view, B: color, R: resolution, D: depth | static asset inspection";
        SetWindowTextA(window, text.c_str());
    }
    void paint_client(HDC target, RECT client, HWND window = nullptr) {
        if (client.right <= 0 || client.bottom <= 0)
            return;
        const auto view = web::presentation_viewport(screen.width, screen.height, client.right, client.bottom, display.scaling);
        const auto dots = game && !show_depth && !presenting_nudge ? web::presented_dmd(view, screen.width, screen.height, web::DmdRaster::layout(variant)) : std::nullopt;
        const web::PresentationFrame frame{
            screen,
            dots ? &renderer->display_backdrop() : nullptr,
            web::DmdRaster::layout(variant),
            view,
            dots,
            options.visual_nudge() ? nudge->offset(variant) : web::NudgeOffset{},
            diagnostics.overlay ? std::function<void(HDC, int)>{[this](HDC dc, int width) { debug_overlay(dc, width, 0); }}
                : std::function<void(HDC, int)>{},
            {view.x + 8, view.y + 8, std::max(1, std::min(view.width - 16, 464)), 56}
        };
        table_presentation.paint(game ? window : nullptr, target, client, frame, reports);
        gpu_presented = table_presentation.gpu_active();
    }
    void paint(HWND window) {
        PAINTSTRUCT paint_state;
        HDC target = BeginPaint(window, &paint_state);
        try {
            RECT client;
            GetClientRect(window, &client);
            paint_client(target, client, window);
        } catch (...) {
            EndPaint(window, &paint_state);
            throw;
        }
        EndPaint(window, &paint_state);
        if (game && frame_pending) {
            const auto now = std::chrono::steady_clock::now();
            const auto row = timing.presented(nanoseconds(now), nanoseconds(now) - nanoseconds(frame_work_start), frame_steps, game->frames());
            if (!diagnostics.frame_log.empty())
                frame_log.push_back(row);
            frame_pending = false;
        }
    }
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
        auto self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        return self ? self->handle_message(window, message, w, l) : DefWindowProcW(window, message, w, l);
    }
    void update_keyboard_key(HWND window, unsigned key, bool down) {
        // A key held across game replacement must be released before it can
        // affect the new game, including Windows auto-repeat key-down messages.
        if (key < restart_held_keys.size() && restart_held_keys[key]) {
            if (!down)
                restart_held_keys[key] = false;
            return;
        }
        if (game) {
            if (down && key < keys.size())
                last_key = key;
            if (key < keys.size())
                keys[key] = down;
            web::windows::combine_modifiers(keys);
            const auto previous = actions;
            actions = edition.consolidated_menu ? host_settings.keyboard_actions(keys) & ~web::host_action::commands : options.actions(keys);
            pending_actions |= actions & ~previous;
            if (edition.timestamped_input && running(window)) {
                const auto received = nanoseconds(std::chrono::steady_clock::now());
                const auto timestamp = InSendMessageEx(nullptr) ? received : dispatched_input_time.value_or(received);
                input_timeline.record({timestamp, web::InputSource::keyboard, actions});
            }
        }
    }
    void apply_raw_keyboard(HWND window) {
        const auto& next = raw_keyboard.held();
        // Apply releases before presses when taking over from legacy input or
        // removing a device. All transitions retain the packet's timestamp.
        for (unsigned key = 0; key < keys.size(); ++key)
            if (!web::windows::generic_modifier(key) && !next[key] && (keys[key] || restart_held_keys[key]))
                update_keyboard_key(window, key, false);
        for (unsigned key = 0; key < keys.size(); ++key)
            if (!web::windows::generic_modifier(key) && next[key] && !keys[key])
                update_keyboard_key(window, key, true);
        for (const unsigned key : {VK_SHIFT, VK_CONTROL, VK_MENU})
            if (!next[key])
                restart_held_keys[key] = false;
    }
    void raw_keyboard_failure(HWND window, DWORD error) {
        RAWINPUTDEVICE device{0x01, 0x06, RIDEV_REMOVE, nullptr};
        RegisterRawInputDevices(&device, 1, sizeof(device));
        raw_keyboard_registered = false;
        raw_keyboard = {};
        for (unsigned key = 0; key < keys.size(); ++key)
            if (keys[key] || restart_held_keys[key])
                update_keyboard_key(window, key, false);
        reports.report("Raw keyboard unavailable; using Windows input", std::system_category().message(int(error)));
    }
    void read_raw_keyboard(HWND window, HRAWINPUT handle) {
        if (!raw_keyboard_registered)
            return;
        RAWINPUT packet{};
        UINT size = sizeof(packet);
        const auto bytes = GetRawInputData(handle, RID_INPUT, &packet, &size, sizeof(RAWINPUTHEADER));
        if (bytes == UINT(-1)) {
            raw_keyboard_failure(window, GetLastError());
            return;
        }
        if (bytes < sizeof(RAWINPUTHEADER) + sizeof(RAWKEYBOARD) || packet.header.dwType != RIM_TYPEKEYBOARD || !focused || GetFocus() != window)
            return;
        if (raw_keyboard.update(packet.header.hDevice, packet.data.keyboard)) {
            if (const auto transition = web::windows::raw_key(packet.data.keyboard); transition && transition->down && !restart_held_keys[transition->key])
                last_key = transition->key; // Retain Classic's repeat-key volume feedback.
            apply_raw_keyboard(window);
            if (packet.data.keyboard.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE)
                reports.report("Keyboard key limit reached", "Windows reported KEYBOARD_OVERRUN_MAKE_CODE; released this keyboard's held controls");
        }
    }
    LRESULT handle_key_message(HWND window, UINT message, WPARAM w, LPARAM l) {
        const bool down = web::windows::keyboard_pressed(message);
        w = web::windows::distinct_key(unsigned(w), l);
        const bool suppressed = w < restart_held_keys.size() && restart_held_keys[w];
        if (!raw_keyboard.active())
            update_keyboard_key(window, unsigned(w), down);
        if (suppressed)
            return 0;
        if (w == 'F')
            held[0] = down;
        if (w == 'J')
            held[1] = down;
        if (w == 'U')
            held[2] = down;
        if (down && !(l & previous_key_down_bit)) {
            bool reload = false;
            if (w == VK_ESCAPE || (edition.consolidated_menu && w == host_settings.keys[10])) {
                if (game && (edition.pause_on_escape || !diagnostics.direct_table))
                    show_backbox(window, edition.pause_on_escape ? web::BackboxEntry::paused_session : web::BackboxEntry::main_menu);
                else
                    SendMessageW(window, WM_CLOSE, 0, 0);
                return 0;
            }
            if ((edition.consolidated_menu ? w == host_settings.keys[8] : (w == 'P' || w == VK_PAUSE)) && game) {
                paused = !paused;
                if (nudge->offset(variant) != web::NudgeOffset{})
                    InvalidateRect(window, nullptr, FALSE);
                reset_pacing();
                title(window);
            }
            if (w == VK_F3 && game) {
                diagnostics.overlay = !diagnostics.overlay;
                InvalidateRect(window, nullptr, FALSE);
            }
            if (w == VK_F2 && game && !edition.consolidated_menu)
                show_options(window);
            if (w == VK_F4 && game && !edition.consolidated_menu)
                show_backbox(window);
            if (w == 'B' && !game) {
                variant ^= 0x40;
                reload = true;
            }
            if (edition.consolidated_menu && game ? w == host_settings.keys[9] : w == 'V') {
                change_view(window);
                return 0;
            }
            if (w == 'R' && !game) {
                unsigned mode = variant >> 4;
                mode = (mode & 4) | ((mode & 3) % 3 + 1);
                variant = mode * 16 + (variant & 15);
                reload = true;
            }
            if (reload) {
                load_view();
                compose();
                fit_pixel_window(window);
                reset_pacing();
                title(window);
                InvalidateRect(window, nullptr, FALSE);
            }
        }
        return 0;
    }
    bool system_game_key(WPARAM value, LPARAM flags) const {
        if (!game)
            return false;
        const auto key = web::windows::distinct_key(unsigned(value), flags);
        if (key >= keys.size())
            return false;
        if (keys[key] || restart_held_keys[key])
            return true;
        if (edition.consolidated_menu)
            return std::find(host_settings.keys.begin(), host_settings.keys.end(), key) != host_settings.keys.end();
        std::array<bool, 256> candidate{};
        candidate[key] = true;
        web::windows::combine_modifiers(candidate);
        return options.actions(candidate) != 0;
    }
    LRESULT handle_message(HWND window, UINT message, WPARAM w, LPARAM l) {
        try {
            if (const auto result = web::development::inspect(*this, message, w))
                return *result;
            switch (message) {
                case WM_SYSCOMMAND:
                    if (game && (w & 0xfff0) == SC_KEYMENU)
                        return 0; // The table owns its menus; bare Alt/F10 cannot capture gameplay input.
                    break;
                case WM_INPUT:
                    read_raw_keyboard(window, reinterpret_cast<HRAWINPUT>(l));
                    return DefWindowProcW(window, message, w, l);
                case WM_INPUT_DEVICE_CHANGE:
                    if (w == GIDC_REMOVAL && raw_keyboard.active()) {
                        raw_keyboard.remove(reinterpret_cast<HANDLE>(l));
                        if (focused && GetFocus() == window)
                            apply_raw_keyboard(window);
                    }
                    return 0;
                case WM_SYSKEYDOWN:
                    if (w == VK_RETURN && (l & alt_pressed_bit)) {
                        if (!(l & previous_key_down_bit))
                            set_fullscreen(window, !display.fullscreen);
                        return 0;
                    }
                    if (w != VK_F4 && system_game_key(w, l))
                        return handle_key_message(window, message, w, l);
                    break;
                case WM_SYSKEYUP:
                    if (system_game_key(w, l))
                        return handle_key_message(window, message, w, l);
                    if (w == VK_RETURN)
                        return 0;
                    break;
                case WM_SYSCHAR:
                    if (w == VK_RETURN)
                        return 0; // Alt+Enter is a host command, not a system-menu character.
                    break;
                case WM_SETCURSOR:
                    if (display.fullscreen && LOWORD(l) == HTCLIENT) {
                        SetCursor(nullptr);
                        return TRUE;
                    }
                    break;
                case WM_TIMER: {
                    if (game)
                        return 0; // Play uses a message-aware frame deadline, not WM_TIMER.
                    bool changed = false;
                    for (int i = 0; i < 3; i++) {
                        auto next = std::clamp(flippers[i] + (held[i] ? 1 : -1), 0, 11);
                        changed |= next != flippers[i];
                        flippers[i] = next;
                    }
                    if (changed) {
                        compose();
                        InvalidateRect(window, nullptr, FALSE);
                    }
                    return 0;
                }
                case WM_KEYDOWN:
                case WM_KEYUP:
                    return handle_key_message(window, message, w, l);
                case WM_KILLFOCUS:
                    raw_keyboard.clear();
                    keys.fill(false);
                    last_key = 0;
                    held.fill(false);
                    actions = 0;
                    pending_actions = 0;
                    focused = false;
                    if (nudge->offset(variant) != web::NudgeOffset{})
                        InvalidateRect(window, nullptr, FALSE);
                    reset_pacing();
                    title(window);
                    return 0;
                case WM_SETFOCUS:
                    focused = true;
                    reset_pacing();
                    title(window);
                    return 0;
                case WM_ENTERSIZEMOVE:
                    sizing = true;
                    reset_pacing();
                    return 0;
                case WM_EXITSIZEMOVE:
                    sizing = false;
                    if (MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) != display_monitor)
                        fit_pixel_window(window);
                    reset_pacing();
                    return 0;
                case WM_ERASEBKGND:
                    return 1; // WM_PAINT supplies the whole client, including the bars.
                case WM_SIZE:
                    InvalidateRect(window, nullptr, FALSE);
                    return 0;
                case WM_GETMINMAXINFO:
                    if (!fitting_window && window_width && window_height) {
                        auto& limits = *reinterpret_cast<MINMAXINFO*>(l);
                        limits.ptMinTrackSize = {window_width, window_height};
                        limits.ptMaxTrackSize = limits.ptMinTrackSize;
                        return 0;
                    }
                    break;
                case WM_WINDOWPOSCHANGING:
                    if (!fitting_window && window_width && window_height && !IsIconic(window) && !(GetWindowLongPtrW(window, GWL_STYLE) & WS_MINIMIZE)) {
                        auto& position = *reinterpret_cast<WINDOWPOS*>(l);
                        if (!(position.flags & SWP_NOSIZE)) {
                            position.cx = window_width;
                            position.cy = window_height;
                        }
                    }
                    break;
                case WM_WINDOWPOSCHANGED:
                    if (!fitting_window && !sizing && window_width && !IsIconic(window) && MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) != display_monitor)
                        fit_pixel_window(window);
                    break;
                case WM_DPICHANGED:
                    if (!fitting_window && window_width) {
                        const auto& suggested = *reinterpret_cast<RECT*>(l);
                        fit_pixel_window(window, MonitorFromRect(&suggested, MONITOR_DEFAULTTONEAREST), LOWORD(w));
                    }
                    return 0;
                case WM_DISPLAYCHANGE:
                    presentation_monitor = nullptr;
                    table_presentation.reset_device();
                    gpu_presented = false;
                    [[fallthrough]];
                case WM_SETTINGCHANGE:
                    if (!fitting_window && window_width && (message == WM_DISPLAYCHANGE || w == SPI_SETWORKAREA))
                        fit_pixel_window(window);
                    return 0;
                case WM_DEVICECHANGE:
                    if (gamepad && w == DBT_DEVNODES_CHANGED)
                        gamepad->devices_changed();
                    return TRUE;
                case WM_PAINT:
                    paint(window);
                    return 0;
                case WM_PRINTCLIENT: {
                    RECT client{};
                    GetClientRect(window, &client);
                    paint_client(reinterpret_cast<HDC>(w), client);
                    return 0;
                }
                case WM_CLOSE:
                    present_exit_display(window);
                    DestroyWindow(window);
                    return 0;
                case WM_DESTROY:
                    if (raw_keyboard_registered) {
                        RAWINPUTDEVICE device{0x01, 0x06, RIDEV_REMOVE, nullptr};
                        RegisterRawInputDevices(&device, 1, sizeof(device));
                        raw_keyboard_registered = false;
                    }
                    checkpoint_scores();
                    checkpoint_options();
                    if (game)
                        game->sound_sink(nullptr);
                    audio.reset();
                    music.reset();
                    KillTimer(window, 1);
                    table_presentation.reset();
                    PostQuitMessage(0);
                    return 0;
            }
        } catch (const std::exception& error) {
            MessageBoxA(window, error.what(), "The Web reconstruction", MB_ICONERROR);
            DestroyWindow(window);
            return 0;
        }
        return DefWindowProcW(window, message, w, l);
    }
    int run_window(bool preview) {
        if (!diagnostics.music_log.empty()) {
            music_trace.open(diagnostics.music_log);
            if (!music_trace)
                throw std::runtime_error("Cannot write music trace");
            music_trace << "timestamp_ns,game_frame,logical_track,revision,cd_track,player,live_balls,station_state,station_captures,video_active,mission_active,paused,phase,mission_time\n";
        }
        // Physical client pixels must remain independent of the desktop's UI
        // scale; non-client decorations still use the current monitor's DPI.
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        host_settings.fullscreen = display.fullscreen;
        display.scaling = web::PresentationScaling::fit;
        if (edition.consolidated_menu && !preview)
            gamepad = std::make_unique<web::GamepadInput>();
        if (!preview) {
            web::display_mode(variant); // Reject an invalid explicit format before creating the game/window.
            game = edition.create_game(assets);
            if (!diagnostics.direct_table)
                game->initialize_engine_startup();
            load_persistence();
            stable_scores = game->save_scores();
            if (diagnostics.explicit_variant)
                options.display_variant(variant);
            edition.normalize_options(options);
            variant = options.display_variant();
            configure_audio();
        }
        load_view();
        compose();
        auto instance = GetModuleHandleW(nullptr);
        WNDCLASSW klass{};
        klass.lpfnWndProc = window_proc;
        klass.hInstance = instance;
        klass.lpszClassName = L"TheWebNativePreview";
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        RegisterClassW(&klass);
        POINT pointer{};
        GetCursorPos(&pointer);
        MONITORINFO monitor{};
        monitor.cbSize = sizeof(monitor);
        const auto target_monitor = MonitorFromPoint(pointer, MONITOR_DEFAULTTOPRIMARY);
        if (!GetMonitorInfoW(target_monitor, &monitor))
            throw std::runtime_error("Cannot query launch monitor");
        auto window = CreateWindowExW(0, klass.lpszClassName, L"The Web", window_style, monitor.rcWork.left, monitor.rcWork.top, screen.width, screen.height, nullptr, nullptr, instance, this);
        if (!window)
            throw std::runtime_error("Cannot create native window");
        if (game) {
            RAWINPUTDEVICE device{0x01, 0x06, RIDEV_DEVNOTIFY, window};
            raw_keyboard_registered = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
            if (!raw_keyboard_registered)
                raw_keyboard_failure(window, GetLastError());
        }
        const bool start_fullscreen = display.fullscreen;
        display.fullscreen = false;
        fit_pixel_window(window, target_monitor);
        if (start_fullscreen)
            set_fullscreen(window, true);
        title(window);
        if (game && !diagnostics.direct_table) {
            show_backbox(window, web::BackboxEntry::startup);
            if (!IsWindow(window))
                return 0;
        }
        last_tick = std::chrono::steady_clock::now();
        ShowWindow(window, SW_SHOW);
        int result = 0;
        if (game)
            result = game_loop(window);
        else {
            SetTimer(window, 1, 16, nullptr);
            MSG message{};
            while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
        }
        save_frame_log();
        return result;
    }
};

namespace web {
NativeApplication::NativeApplication(std::filesystem::path assets, unsigned variant, const Edition& edition) :
    impl_(std::make_unique<Impl>(std::move(assets), variant, edition)) {}
NativeApplication::~NativeApplication() = default;
int NativeApplication::run(bool depth, bool preview, ApplicationDiagnostics diagnostics, ApplicationPersistence persistence, WindowDisplay display) {
    impl_->show_depth = diagnostics.allow_depth_view && depth;
    impl_->diagnostics = std::move(diagnostics);
    impl_->persistence = std::move(persistence);
    impl_->display = display;
    return impl_->run_window(preview);
}
void NativeApplication::export_bitmap(const std::filesystem::path& path, bool sprites, bool depth) {
    impl_->show_depth = depth;
    impl_->load_view();
    impl_->compose(sprites);
    write_bmp(path, impl_->screen);
}
} // namespace web
