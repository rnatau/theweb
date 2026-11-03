#pragma once
#include "../../options.hpp"
#include <functional>

namespace web {
struct Edition;
struct HostSettings;
class DiagnosticLog;
enum class BackboxEntry {
    overlay,
    startup,
    main_menu,
    paused_session
};
// Startup includes the original timed logos. Home-menu Escape/close quits;
// Enhanced also quits from a retained-session menu. Classic overlay Escape
// returns to its table. Enhanced resumes only through the RESUME menu item.
// Classic home PLAY starts a fresh engine, matching MENU Q -> WGAME startup.
// prepare_play runs under the visible menu so its owner can paint before close.
bool show_backbox(
    void* owner,
    const std::filesystem::path& assets,
    GameOptions& options,
    bool audio_forced_off,
    const Edition& edition,
    BackboxEntry entry = BackboxEntry::overlay,
    HostSettings* settings = nullptr,
    DiagnosticLog* reports = nullptr,
    std::function<void()> prepare_play = {});
} // namespace web
