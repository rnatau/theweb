#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>

namespace web {
struct MusicRange {
    unsigned first = 0, last = 0; // Inclusive physical CD track numbers.
};
class MusicCatalog {
public:
    explicit MusicCatalog(const std::filesystem::path& assets);
    static MusicCatalog from_directory(std::filesystem::path);
    bool available() const;
    const std::filesystem::path& directory() const {
        return directory_;
    }
    std::filesystem::path file(unsigned cd_track) const;
    static std::optional<MusicRange> range(int logical_track);

private:
    MusicCatalog() = default;
    std::filesystem::path directory_;
};
struct MusicPlayback {
    int track = -1;
    std::uint64_t revision = 0;
    int fade_timer = 0; // Original transition wait; Windows fade hook is empty.
};
// Preserve the device's current request across an Enhanced game replacement.
// The old range finishes naturally unless the fresh game requests another theme.
class MusicContinuation {
public:
    void begin(MusicPlayback playing, MusicPlayback fresh) {
        retained_ = playing.track >= 0 ? std::optional(playing) : std::nullopt;
        baseline_ = fresh;
    }
    MusicPlayback playback(MusicPlayback current) {
        if (retained_ && !same(current, baseline_))
            retained_.reset();
        return retained_.value_or(current);
    }
    MusicPlayback completed(MusicPlayback event, MusicPlayback current) {
        playback(current);
        if (retained_ && same(event, *retained_)) {
            retained_.reset();
            return current;
        }
        return event;
    }

private:
    static bool same(MusicPlayback a, MusicPlayback b) {
        return a.track == b.track && a.revision == b.revision;
    }
    std::optional<MusicPlayback> retained_;
    MusicPlayback baseline_;
};
} // namespace web
