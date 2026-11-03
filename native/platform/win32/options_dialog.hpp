#pragma once
#include "../../options.hpp"
#include "window_display.hpp"

namespace web {
struct Edition;
// Modal native presentation; the caller suspends simulation and audio.
bool edit_options(void* owner, GameOptions& options, bool audio_forced_off, const std::filesystem::path& assets, WindowDisplay& display, const Edition& edition);
} // namespace web
