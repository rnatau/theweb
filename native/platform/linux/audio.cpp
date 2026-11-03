#include "audio.hpp"
#include "../../sound_pcm.hpp"
#include <SDL.h>
#include <mpg123.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <utility>

namespace web {
namespace {
SDL_AudioDeviceID device(unsigned rate, SDL_AudioCallback callback = nullptr, void* userdata = nullptr, SoundPcmFormat format = {}) {
    SDL_AudioSpec desired{};
    desired.freq = int(rate);
    desired.format = format.sixteen_bit ? AUDIO_S16LSB : AUDIO_U8;
    desired.channels = Uint8(format.channels());
    desired.samples = 512;
    desired.callback = callback;
    desired.userdata = userdata;
    const auto result = SDL_OpenAudioDevice(nullptr, 0, &desired, nullptr, 0);
    if (!result)
        throw std::runtime_error(std::string("Cannot open audio: ") + SDL_GetError());
    return result;
}
} // namespace
class LinuxEffects::Impl {
public:
    SoundBank bank;
    SoundMixer mixer;
    mutable std::mutex mutex;
    SDL_AudioDeviceID output = 0;
    SoundPcmFormat format;
    std::array<std::int16_t, 1024> mixed{};
    std::atomic<std::uint64_t> frames = 0;
    Impl(const std::filesystem::path& root, const GameOptions& options, SoundRequestFilter filter) :
        bank(SoundBank::load(root, options.sound_rate())),
        mixer(bank, options.sound_channels(), filter),
        format{options.sound_stereo(), options.sound_16bit()} {
        mixer.volume(options.volume());
        output = device(
            bank.rate,
            [](void* data, Uint8* bytes, int size) {
                auto& self = *static_cast<Impl*>(data);
                std::lock_guard guard(self.mutex);
                const auto samples = std::size_t(size) / (self.format.channels() * self.format.bits() / 8) * 2;
                if (samples > self.mixed.size()) {
                    std::fill_n(bytes, size, self.format.sixteen_bit ? 0 : 128);
                    return;
                }
                const auto mixed = std::span(self.mixed).first(samples);
                self.mixer.render(mixed);
                self.format.encode(mixed, {bytes, std::size_t(size)});
                self.frames += samples / 2;
            },
            this,
            format);
    }
    ~Impl() {
        SDL_CloseAudioDevice(output);
    }
};
LinuxEffects::LinuxEffects(const std::filesystem::path& root, const GameOptions& options, SoundRequestFilter filter) :
    impl_(std::make_unique<Impl>(root, options, filter)) {}
LinuxEffects::~LinuxEffects() = default;
void LinuxEffects::sound(SoundRequest request) {
    std::lock_guard guard(impl_->mutex);
    impl_->mixer.sound(request);
}
void LinuxEffects::stop(int id, int channel, int group) {
    std::lock_guard guard(impl_->mutex);
    impl_->mixer.stop(id, channel, group);
}
bool LinuxEffects::active(int id, int channel, int group) const {
    std::lock_guard guard(impl_->mutex);
    return impl_->mixer.active(id, channel, group);
}
void LinuxEffects::volume(unsigned value) {
    std::lock_guard guard(impl_->mutex);
    impl_->mixer.volume(value);
}
void LinuxEffects::pause(bool value) {
    SDL_PauseAudioDevice(impl_->output, value);
}
std::uint64_t LinuxEffects::rendered_frames() const {
    return impl_->frames;
}

// MP3 decoding and device filling are independent of simulation/rendering.
// The bounded queue carries at most ~70 ms, including track-to-track ranges.
class LinuxMusic::Impl {
public:
    MusicCatalog catalog;
    SDL_AudioDeviceID output = 0;
    mpg123_handle* decoder = nullptr;
    std::mutex mutex;
    MusicPlayback request;
    std::optional<MusicPlayback> completion;
    unsigned volume = 0, track = 0, last = 0;
    bool paused = true, changed = false, draining = false;
    std::jthread worker;
    std::atomic<std::uint64_t> frames = 0;
    explicit Impl(const std::filesystem::path& root) :
        catalog(root) {
        static const int initialized = mpg123_init();
        if (initialized != MPG123_OK)
            throw std::runtime_error("Cannot initialize MP3 decoder");
        decoder = mpg123_new(nullptr, nullptr);
        if (!decoder)
            throw std::runtime_error("Cannot create MP3 decoder");
        mpg123_param(decoder, MPG123_ADD_FLAGS, MPG123_FORCE_STEREO, 0);
        mpg123_param(decoder, MPG123_FORCE_RATE, 44100, 0);
        mpg123_format_none(decoder);
        mpg123_format(decoder, 44100, MPG123_STEREO, MPG123_ENC_SIGNED_16);
        try {
            output = device(44100);
            worker = std::jthread([this](std::stop_token stop) { run(stop); });
        } catch (...) {
            if (output)
                SDL_CloseAudioDevice(output);
            mpg123_delete(decoder);
            throw;
        }
    }
    ~Impl() {
        worker.request_stop();
        worker.join();
        SDL_CloseAudioDevice(output);
        mpg123_delete(decoder);
    }
    void open() {
        mpg123_close(decoder);
        if (mpg123_open(decoder, catalog.file(track).c_str()) != MPG123_OK)
            throw std::runtime_error("Cannot decode music " + catalog.file(track).string());
    }
    void run(std::stop_token stop) {
        std::array<std::int16_t, 2048> pcm{};
        while (!stop.stop_requested()) {
            {
                std::lock_guard guard(mutex);
                try {
                    if (changed) {
                        changed = false;
                        completion.reset();
                        SDL_ClearQueuedAudio(output);
                        mpg123_close(decoder);
                        track = last = 0;
                        draining = false;
                        if (const auto range = MusicCatalog::range(request.track)) {
                            track = range->first;
                            last = range->last;
                            open();
                        }
                    }
                    if (!paused && track && SDL_GetQueuedAudioSize(output) < 8192) {
                        if (draining) {
                            if (!SDL_GetQueuedAudioSize(output)) {
                                completion = request;
                                track = 0;
                            }
                        } else {
                            std::size_t bytes = 0;
                            const auto result = mpg123_read(decoder, reinterpret_cast<unsigned char*>(pcm.data()), sizeof(pcm), &bytes);
                            if (result != MPG123_OK && result != MPG123_DONE && result != MPG123_NEW_FORMAT)
                                throw std::runtime_error(mpg123_strerror(decoder));
                            for (std::size_t i = 0; i < bytes / sizeof(std::int16_t); ++i)
                                pcm[i] = std::int16_t(int(pcm[i]) * int(volume) / 255);
                            if (bytes && SDL_QueueAudio(output, pcm.data(), Uint32(bytes)) < 0)
                                throw std::runtime_error(SDL_GetError());
                            frames += bytes / (2 * sizeof(std::int16_t));
                            if (result == MPG123_DONE) {
                                if (track < last) {
                                    ++track;
                                    open();
                                } else
                                    draining = true;
                            }
                        }
                    }
                } catch (const std::exception& error) {
                    std::cerr << error.what() << " | continuing without this music range\n";
                    completion = request;
                    track = 0;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
};
LinuxMusic::LinuxMusic(const std::filesystem::path& root) :
    impl_(std::make_unique<Impl>(root)) {}
LinuxMusic::~LinuxMusic() = default;
void LinuxMusic::update(MusicPlayback request, unsigned volume) {
    std::lock_guard guard(impl_->mutex);
    impl_->volume = volume;
    if (request.track != impl_->request.track || request.revision != impl_->request.revision) {
        impl_->request = request;
        impl_->changed = true;
    }
}
void LinuxMusic::pause(bool value) {
    std::lock_guard guard(impl_->mutex);
    impl_->paused = value;
    SDL_PauseAudioDevice(impl_->output, value);
}
std::optional<MusicPlayback> LinuxMusic::completed() {
    std::lock_guard guard(impl_->mutex);
    return std::exchange(impl_->completion, std::nullopt);
}
std::uint64_t LinuxMusic::submitted_frames() const {
    return impl_->frames;
}
} // namespace web
