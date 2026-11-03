#pragma once
#include <filesystem>

namespace web {
std::filesystem::path executable_directory();
// Portable/repository locations relative to the executable; never the CWD.
std::filesystem::path default_assets_directory();
} // namespace web
