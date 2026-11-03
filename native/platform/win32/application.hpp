#pragma once
#include "window_display.hpp"
#include <filesystem>
#include <memory>

namespace web {
struct Edition;
struct ApplicationDiagnostics {
    bool overlay = false;
    bool mute = false;
    bool explicit_variant = false;
    bool direct_table = false;      // Inspection/testing shortcut; normal launch starts at the frontend.
    bool allow_depth_view = false;  // Only the unshipped development entry point enables this.
    unsigned presentation_rate = 0; // Development timing fixture; zero follows the monitor.
    std::filesystem::path frame_log;
    std::filesystem::path music_log; // Development request/file/mode trace.
};
struct ApplicationPersistence {
    bool enabled = true;
    bool edition_subdirectory = true; // Shipping save roots always isolate editions.
    std::filesystem::path directory;  // Empty selects saves/ beside the executable; edition subfolder is appended.
};
// Owns game/preview presentation and Win32/GDI resources. The core has no
// window, timer or device dependencies and can be tested headlessly.
class NativeApplication {
public:
    NativeApplication(std::filesystem::path assets, unsigned variant, const Edition&);
    ~NativeApplication();
    NativeApplication(const NativeApplication&) = delete;
    NativeApplication& operator=(const NativeApplication&) = delete;
    int run(bool depth = false, bool preview = false, ApplicationDiagnostics diagnostics = {}, ApplicationPersistence persistence = {}, WindowDisplay display = {});
    void export_bitmap(const std::filesystem::path& path, bool sprites = false, bool depth = false);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace web
