#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "application.hpp"
#include "application_paths.hpp"
#include "../../web.hpp"
#include "../../edition.hpp"
#include "../../render/table_renderer.hpp"
#include "../../incremental_session.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

int main(int argc, char** argv) {
    bool interactive = false;
    try {
        const auto& edition = web::selected_edition();
        std::filesystem::path assets;
        bool explicit_assets = false;
        unsigned variant = 0x70;
        bool show_depth = false;
        std::string action, output, input, snapshot;
        bool overlay = false, preview = false;
        web::ApplicationDiagnostics diagnostics;
        diagnostics.allow_depth_view = true;
        web::ApplicationPersistence persistence;
        persistence.edition_subdirectory = false; // Exact paths are required by disk fixtures.
        web::WindowDisplay display;
        bool explicit_window_mode = false;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            auto next = [&]() {
                if (++i >= argc)
                    throw std::runtime_error("Missing argument for " + arg);
                return std::string(argv[i]);
            };
            if (arg == "--edition") {
                std::cout << edition.id << "\n";
                return 0;
            }
            if (arg == "--version") {
                std::cout << edition.title << " " << edition.version << " | " << edition.id << " | rules " << edition.rules_version << " | physics " << edition.physics_version << " | input "
                          << edition.input_version << '\n';
                return 0;
            }
            if (arg == "--assets") {
                assets = next();
                explicit_assets = true;
            } else if (arg == "--variant") {
                variant = std::stoul(next(), nullptr, 16);
                diagnostics.explicit_variant = true;
            } else if (arg == "--sprites")
                overlay = true;
            else if (arg == "--depth")
                show_depth = true;
            else if (arg == "--preview")
                preview = true;
            else if (arg == "--table")
                diagnostics.direct_table = true;
            else if (arg == "--no-debug")
                diagnostics.overlay = false;
            else if (arg == "--debug")
                diagnostics.overlay = true;
            else if (arg == "--fullscreen" || arg == "--windowed") {
                display.fullscreen = arg == "--fullscreen";
                explicit_window_mode = true;
                display.explicit_mode = true;
            } else if (arg == "--frame-log")
                diagnostics.frame_log = next();
            else if (arg == "--music-log")
                diagnostics.music_log = next();
            else if (arg == "--presentation-hz") {
                diagnostics.presentation_rate = unsigned(std::stoul(next()));
                if (diagnostics.presentation_rate < 20 || diagnostics.presentation_rate > 1000 || !edition.incremental_presentation)
                    throw std::runtime_error("Presentation rate fixture requires Enhanced and 20..1000 Hz");
            } else if (arg == "--save-dir")
                persistence.directory = next();
            else if (arg == "--no-save")
                persistence.enabled = false;
            else if (arg == "--mute")
                diagnostics.mute = true;
            else if (arg == "--snapshot")
                snapshot = next();
            else if (arg == "--validate-assets")
                action = arg;
            else if (arg == "--default-config" || arg == "--bmp" || arg == "--collision-grid") {
                action = arg;
                output = next();
            } else if (arg == "--decode-scores" || arg == "--encode-scores" || arg == "--nudge-trace" || arg == "--replay" || arg == "--replay-timed") {
                action = arg;
                input = next();
                output = next();
            } else if (arg == "--help") {
                std::cout << "The Web: game session using recovered integer physics.\n"
                             "Normal launch: original startup/menu; PLAY opens the table; Esc returns to the menu.\n"
                             "S: start/add player (up to four), Enter: launch/skip bonus, F/J: flippers, C: continue, Space/G/H: nudge, P: pause, V: view\n"
                             "Table arrows: original effects volume; --table only skips startup/menu.\n"
                             "--assets data/PC_DAT --variant 70 [--table | --preview] [--bmp file.bmp --sprites] [--depth]\n"
                             "--replay actions.bin trace.csv [--snapshot frame.bmp] (one little-endian action DWORD per 60Hz tick)\n"
                             "--replay-timed events.twi trace.csv [--snapshot frame.bmp] (Enhanced TWI1 batch / TWI2 incremental input)\n"
                             "F2: game options | F3: frame/FPS/jitter overlay | F4: original backbox | --no-debug | --frame-log frame-times.csv\n"
                             "Alt+Enter: fullscreen | --fullscreen | --windowed | --debug\n"
                             "--music-log music.csv (music requests, actual files and mode transitions)\n"
                             "--save-dir folder | --no-save (GUI only; replay/preview never save)\n"
                             "--mute (disable sound output) | --edition (print build identity)\n"
                             "--validate-assets | --default-config file | --collision-grid file\n"
                             "--decode-scores input output | --encode-scores input output | --nudge-trace input output\n";
                return 0;
            } else
                throw std::runtime_error("Unknown argument: " + arg);
        }
        const auto assets_root = [&]() -> const std::filesystem::path& {
            if (!explicit_assets && assets.empty())
                assets = web::default_assets_directory();
            return assets;
        };
        if (action == "--validate-assets") {
            auto count = web::validate_assets(assets_root());
            std::cout << "Validated " << count << " sprite/mask frames and all table/depth/collision variants\n";
            return 0;
        }
        if (action == "--default-config") {
            web::write_file(output, web::serialize_configuration(web::default_configuration()));
            return 0;
        }
        if (action == "--nudge-trace") {
            web::write_file(output, web::nudge_trace(web::read_file(input)));
            return 0;
        }
        if (action == "--replay" || action == "--replay-timed") {
            auto bytes = web::read_file(input);
            const bool timed = action == "--replay-timed";
            if (timed && !edition.timestamped_input)
                throw std::runtime_error("Timestamped input replays require the Enhanced input contract");
            const auto recording = timed ? web::parse_timed_input(bytes) : web::TimedInputReplay{};
            web::InputTimeline timeline;
            web::IncrementalSession live;
            for (const auto& event : recording.events)
                timeline.record(event);
            if (!timed && bytes.size() % 4)
                throw std::runtime_error("Replay needs whole action DWORDs");
            auto session = edition.create_game(assets_root());
            auto& game = *session;
            if (timed && recording.version == 2)
                live.reset(0, game);
            std::unique_ptr<web::TableRenderer> replay_renderer;
            if (!snapshot.empty()) {
                replay_renderer = std::make_unique<web::TableRenderer>(assets_root(), variant);
                replay_renderer->enable_lamp_timing();
            }
            std::string trace = "frame,phase,ball,slot,active,type,mode,layer,x,y,z,vx,vy,launches,drains,hits,score\n";
            const auto frames = timed ? recording.frames : bytes.size() / 4;
            for (std::size_t frame = 0; frame < frames; ++frame) {
                if (timed && recording.version == 2) {
                    live.advance(((frame + 1) * 1000000000ULL + 59) / 60, game, timeline, [&] { game.begin_presentation(); }, [] { return false; });
                } else if (timed) {
                    game.begin_presentation();
                    const auto& state = game.simulation();
                    game.step_timed(timeline.frame(
                        (frame * 1000000000ULL + 59) / 60,
                        ((frame + 1) * 1000000000ULL + 59) / 60,
                        web::physics::PhysicsSimulation::microstep_count(state.refresh_rate, state.double_steps)));
                } else {
                    game.begin_presentation();
                    game.step(web::u32(bytes, frame * 4));
                }
                if (replay_renderer) {
                    replay_renderer->advance_lamps(game.lamps(), 1, game.lamp_animation_active(), game.take_lamp_redraw());
                    replay_renderer->advance_balls(game.simulation());
                }
                for (unsigned slot = 0; slot < 6; slot++) {
                    const auto& ball = game.simulation().balls[slot];
                    if (!ball.active())
                        continue;
                    trace += std::to_string(frame) + "," + std::to_string(int(game.phase())) + "," + std::to_string(game.ball_number()) + "," + std::to_string(slot) + "," +
                             std::to_string(ball.active()) + "," + std::to_string(unsigned(ball.motion_type())) + "," + std::to_string(unsigned(ball.motion_mode())) + "," +
                             std::to_string(ball.layer()) + "," + std::to_string(ball.position().x) + "," + std::to_string(ball.position().y) + "," + std::to_string(ball.z()) + "," +
                             std::to_string(ball.velocity().x) + "," + std::to_string(ball.velocity().y) + "," + std::to_string(game.launches()) + "," + std::to_string(game.drains()) + "," +
                             std::to_string(game.interactions()) + "," + std::to_string(game.score().points()) + "\n";
                }
            }
            web::write_file(output, std::span(reinterpret_cast<const std::uint8_t*>(trace.data()), trace.size()));
            if (!snapshot.empty()) {
                web::write_bmp(snapshot, replay_renderer->compose(game.simulation(), false, game.lamps(), &game.dot_matrix_frame()));
            }
            std::cout << game.status() << " | launches " << game.launches() << " | drains " << game.drains() << " | hits " << game.interactions() << '\n';
            return 0;
        }
        if (action == "--decode-scores") {
            auto record = web::parse_scores(web::read_file(input));
            web::write_file(output, record.payload);
            return 0;
        }
        if (action == "--encode-scores") {
            auto payload = web::read_file(input);
            if (payload.size() != 156)
                throw std::runtime_error("Score payload must be 156 bytes");
            web::Scores scores;
            std::copy(payload.begin(), payload.end(), scores.payload.begin());
            web::write_file(output, web::serialize_scores(scores));
            return 0;
        }
        if (action == "--collision-grid") {
            auto map = web::TileMap::load(assets_root() / "COLLIDE/COLLIDE0.MAP", 40);
            web::Bytes pixels;
            for (int y = 0; y < map.rows * 8; y++)
                for (int x = 0; x < map.columns * 8; x++)
                    pixels.push_back(map.sample(x, y));
            web::write_file(output, pixels);
            return 0;
        }
        interactive = action.empty();
        web::NativeApplication application(assets_root(), variant, edition);
        if (action == "--bmp") {
            application.export_bitmap(output, overlay, show_depth);
            return 0;
        }
        if (!explicit_window_mode)
            display.fullscreen = !diagnostics.direct_table && !preview;
        return application.run(show_depth, preview, std::move(diagnostics), std::move(persistence), display);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        if (interactive && GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_UNKNOWN)
            MessageBoxA(nullptr, error.what(), "Pro Pinball: The Web", MB_OK | MB_ICONERROR);
        return 1;
    }
}
