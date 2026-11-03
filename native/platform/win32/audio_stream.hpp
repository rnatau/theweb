#pragma once
#include "com_ptr.hpp"
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <atomic>
#include <cstdint>
#include <span>
#include <string>

namespace web::windows {
// Event-driven shared WASAPI stream. The caller's worker owns all COM use;
// game/rule code continues to provide its original-rate PCM and mixer policy.
class AudioStream {
public:
    AudioStream(unsigned rate, unsigned channels, unsigned bits);
    ~AudioStream();
    AudioStream(const AudioStream&) = delete;
    AudioStream& operator=(const AudioStream&) = delete;
    HANDLE ready() const;
    bool changed();
    void reopen();
    unsigned available();
    std::uint64_t played();
    void write(std::span<const std::uint8_t>);
    void pause(bool);
    void reset();
    unsigned capacity_frames() const {
        return capacity_;
    }

private:
    class Notifications;
    ComPtr<Notifications> notifications_;
    ComPtr<IMMDeviceEnumerator> devices_;
    ComPtr<IAudioClient> client_;
    ComPtr<IAudioRenderClient> render_;
    WAVEFORMATEX format_{};
    unsigned capacity_ = 0;
    bool paused_ = true;
    std::uint64_t written_ = 0, base_ = 0;
    void open();
};
} // namespace web::windows
