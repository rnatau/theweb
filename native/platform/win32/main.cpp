#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include "application.hpp"
#include "application_paths.hpp"
#include "../../edition.hpp"
#include <iostream>
#include <memory>
#include <stdexcept>

int main() {
    bool interactive = false;
    try {
        const auto& edition = web::selected_edition();
        int argument_count = 0;
        const std::unique_ptr<wchar_t*, decltype(&LocalFree)> arguments(CommandLineToArgvW(GetCommandLineW(), &argument_count), &LocalFree);
        if (!arguments)
            throw std::runtime_error("Cannot read the Windows command line");
        web::ApplicationPersistence persistence;
        web::ApplicationDiagnostics diagnostics;
        web::WindowDisplay display;
        display.fullscreen = true;
        for (int i = 1; i < argument_count; ++i) {
            const auto arg = std::filesystem::path(arguments.get()[i]).string();
            const auto next = [&]() {
                if (++i >= argument_count)
                    throw std::invalid_argument("Missing value for " + arg);
                return std::filesystem::path(arguments.get()[i]);
            };
            if (arg == "--help") {
                std::cout << edition.title << " " << edition.version
                          << "\n"
                             "--windowed | --fullscreen  Choose the display mode (default: fullscreen).\n"
                             "--mute                     Disable audio for this run.\n"
                             "--save-dir PATH            Use an alternate save root (edition subfolder added).\n"
                             "--no-save                  Play without loading or writing saved records/settings.\n"
                             "--version                  Show edition and behavioral versions.\n"
                             "Esc opens the game menu; Alt+Enter toggles fullscreen.\n";
                return 0;
            } else if (arg == "--version") {
                std::cout << edition.title << " " << edition.version << " | " << edition.id << " | rules " << edition.rules_version << " | physics " << edition.physics_version << " | input "
                          << edition.input_version << '\n';
                return 0;
            } else if (arg == "--windowed" || arg == "--fullscreen") {
                display.fullscreen = arg == "--fullscreen";
                display.explicit_mode = true;
            } else if (arg == "--mute")
                diagnostics.mute = true;
            else if (arg == "--save-dir")
                persistence.directory = next();
            else if (arg == "--no-save")
                persistence.enabled = false;
            else
                throw std::invalid_argument("Unknown option: " + arg + ". Use --help for launch options.");
        }
        interactive = true;
        const auto assets = web::default_assets_directory();
        web::NativeApplication application(assets, 0x70, edition);
        return application.run(false, false, diagnostics, persistence, display);
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        if (interactive && GetFileType(GetStdHandle(STD_ERROR_HANDLE)) == FILE_TYPE_UNKNOWN)
            MessageBoxA(nullptr, error.what(), "Pro Pinball: The Web", MB_OK | MB_ICONERROR);
        return 1;
    }
}
