#pragma once
#include <filesystem>
#include <optional>

namespace web {
inline std::optional<std::filesystem::path> find_desktop_assets(const std::filesystem::path& directory, const std::filesystem::path& shared_data = {}) {
    for (const auto& candidate : {shared_data.empty() ? std::filesystem::path{} : shared_data / "PC_DAT", directory / "data/PC_DAT", directory / "PC_DAT", directory.parent_path() / "data/PC_DAT"})
        if (!candidate.empty() && std::filesystem::is_regular_file(candidate / "70/TABLE/VIEW.BIT") && std::filesystem::is_regular_file(candidate / "MATRIX/FONTS/FONT1.BIT"))
            return std::filesystem::canonical(candidate);
    return std::nullopt;
}
} // namespace web
