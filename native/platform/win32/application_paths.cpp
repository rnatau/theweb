#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "application_paths.hpp"
#include <array>
#include <stdexcept>
#include <system_error>

namespace web {
std::filesystem::path executable_directory() {
    std::wstring path(32768, L'\0');
    const auto size = GetModuleFileNameW(nullptr, path.data(), DWORD(path.size()));
    if (!size || size == path.size())
        throw std::system_error(int(GetLastError()), std::system_category(), "Cannot find executable directory");
    path.resize(size);
    return std::filesystem::path(path).parent_path();
}
std::filesystem::path default_assets_directory() {
    const auto directory = executable_directory();
    const std::array candidates = {directory / "data/PC_DAT", directory / "PC_DAT", directory.parent_path() / "data/PC_DAT"};
    for (const auto& candidate : candidates) {
        std::error_code error;
        const auto root = std::filesystem::weakly_canonical(candidate, error);
        if (error || !std::filesystem::is_directory(root / "COLLIDE", error) || error || !std::filesystem::is_regular_file(root / "MATRIX/FONTS/FONT1.BIT", error) || error ||
            !std::filesystem::is_regular_file(root / "70/TABLE/VIEW.BIT", error) || error)
            continue;
        return root;
    }
    throw std::runtime_error("Cannot find The Web assets. Place data/PC_DAT beside the executable.");
}
} // namespace web
