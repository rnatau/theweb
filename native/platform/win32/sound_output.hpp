#pragma once
#include "../../sound.hpp"
#include "../../sound_pcm.hpp"
#include <memory>

namespace web {
// The selected edition backend owns device buffers; the game and recovered sound core stay headless.
class SoundOutput : public SoundSink {
public:
    explicit SoundOutput(const std::filesystem::path& assets, unsigned rate = 44100, unsigned channels = 3, SoundPcmFormat = {}, SoundRequestFilter filter = nullptr);
    ~SoundOutput();
    void sound(SoundRequest) override;
    void stop(int id = -1, int channel = -1, int group = -1) override;
    bool active(int id = -1, int channel = -1, int group = -1) const override;
    // Poll worker errors; device completion drives refills independently.
    void pump();
    std::string take_error();
    unsigned buffer_frames() const;
    std::uint64_t recoveries() const;
    void pause(bool paused);
    void volume(unsigned);
    std::uint64_t submitted_buffers() const;
    // Missed refill beyond the device-buffer tolerance; excludes priming/pause.
    std::uint64_t underruns() const;
    unsigned rate() const;
    unsigned channels() const;
    SoundPcmFormat format() const;
    unsigned queued_finished_voices() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace web
