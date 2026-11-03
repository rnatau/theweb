#include "music.hpp"
#include <string>

namespace web {
MusicCatalog::MusicCatalog(const std::filesystem::path& assets) {
    std::error_code error;
    directory_ = assets / "MUSIC";
    if (!std::filesystem::is_directory(directory_, error))
        directory_ = assets.parent_path() / "MUSIC";
}
MusicCatalog MusicCatalog::from_directory(std::filesystem::path directory) {
    MusicCatalog result;
    result.directory_ = std::move(directory);
    return result;
}
bool MusicCatalog::available() const {
    std::error_code error;
    return std::filesystem::is_directory(directory_, error);
}
std::filesystem::path MusicCatalog::file(unsigned track) const {
    return directory_ / ("Track" + std::string(track < 10 ? "0" : "") + std::to_string(track) + ".mp3");
}
std::optional<MusicRange> MusicCatalog::range(int track) {
    // Original WGAME 0x415798: play from track+1 to the next boundary;
    // logical 11, 14 and 20 include two physical CD tracks.
    if (track < 1 || track > 23)
        return {};
    const auto first = unsigned(track + 1);
    return MusicRange{first, first + unsigned(track == 11 || track == 14 || track == 20)};
}
} // namespace web
