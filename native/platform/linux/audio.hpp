#pragma once
#include "../../sound.hpp"
#include "../../music.hpp"
#include "../../options.hpp"
#include <memory>

namespace web {
class LinuxEffects final : public SoundSink {
public:
    LinuxEffects(const std::filesystem::path&, const GameOptions&, SoundRequestFilter);
    ~LinuxEffects();
    void sound(SoundRequest) override;
    void stop(int = -1, int = -1, int = -1) override;
    bool active(int = -1, int = -1, int = -1) const override;
    void pause(bool);
    void volume(unsigned);
    std::uint64_t rendered_frames() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
class LinuxMusic {
public:
    explicit LinuxMusic(const std::filesystem::path&);
    ~LinuxMusic();
    void update(MusicPlayback, unsigned volume);
    void pause(bool);
    std::optional<MusicPlayback> completed();
    std::uint64_t submitted_frames() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace web
