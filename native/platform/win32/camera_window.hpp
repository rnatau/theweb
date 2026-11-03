#pragma once
#include "../../options.hpp"
#include <optional>

namespace web {
// Accepts a draft camera; no persistent setting changes until the owner saves.
std::optional<unsigned> choose_camera(void* owner, const std::filesystem::path& assets, const GameOptions& options, bool audio_forced_off);
} // namespace web
