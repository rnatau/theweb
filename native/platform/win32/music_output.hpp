#pragma once
#include "../../music.hpp"
#include <memory>
#include <string>

namespace web {
// Optional MP3 streaming on a separate completion-driven Windows output device.
// All file/codec/device work stays off the simulation and sound-effects threads.
class MusicOutput {
public:
    explicit MusicOutput(MusicCatalog);
    ~MusicOutput();
    void update(MusicPlayback, unsigned volume);
    void pause(bool);
    std::optional<MusicPlayback> completed();
    std::string take_error();
    unsigned cd_track() const;
    std::uint64_t submitted_buffers() const;
    std::uint64_t submitted_frames() const;
    std::uint64_t playback_starts() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace web
