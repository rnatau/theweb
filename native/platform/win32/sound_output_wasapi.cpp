#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "sound_output.hpp"
#include "audio_stream.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <mutex>
#include <thread>

namespace web {
class SoundOutput::Impl {
public:
    SoundBank bank;
    SoundMixer mixer;
    SoundPcmFormat pcm_format;
    std::atomic<std::uint64_t> submitted = 0, underruns = 0, recoveries = 0;
    std::atomic<unsigned> buffer_frames = 0;
    HANDLE stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    HANDLE wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    std::mutex mutex;
    std::condition_variable condition;
    std::thread worker;
    std::exception_ptr failure;
    std::string error;
    bool opened = false, paused = false, effective_pause = false;
    std::uint64_t submitted_frames = 0;
    std::vector<std::uint64_t> voice_completions;
    explicit Impl(const std::filesystem::path& assets, unsigned rate, unsigned channels, SoundPcmFormat format, SoundRequestFilter filter) :
        bank(SoundBank::load(assets, rate)),
        mixer(bank, std::min(channels, 3u), filter),
        pcm_format(format),
        voice_completions(mixer.voices().size()) {
        if (!stop_event || !wake) {
            if (stop_event)
                CloseHandle(stop_event);
            if (wake)
                CloseHandle(wake);
            throw std::runtime_error("Audio output: cannot create completion events");
        }
        mixer.retain_until_played = true;
    }
    void start() {
        worker = std::thread([this] { run(); });
        std::unique_lock lock(mutex);
        condition.wait(lock, [&] { return opened; });
        rethrow_failure();
    }
    void rethrow_failure() const {
        if (failure)
            std::rethrow_exception(failure);
    }
    void run() noexcept {
        try {
            windows::ComApartment apartment;
            windows::AudioStream stream(bank.rate, pcm_format.channels(), pcm_format.bits());
            std::vector<std::int16_t> mixed;
            Bytes pcm;
            auto last_fill = std::chrono::steady_clock::now();
            bool connected = true;
            const auto refill = [&] {
                const auto played = stream.played();
                for (unsigned channel = 0; channel < voice_completions.size(); ++channel)
                    if (voice_completions[channel] && voice_completions[channel] <= played) {
                        mixer.complete(channel);
                        voice_completions[channel] = 0;
                    }
                auto available = stream.available();
                const auto now = std::chrono::steady_clock::now();
                const auto tolerance = std::chrono::nanoseconds(std::uint64_t(stream.capacity_frames()) * 2000000000 / bank.rate);
                if (submitted && !paused && available == stream.capacity_frames() && now - last_fill > tolerance)
                    ++underruns;
                while (available) {
                    const auto count = std::min(available, 512u);
                    mixed.resize(count * 2);
                    pcm.resize(count * pcm_format.channels() * pcm_format.bits() / 8);
                    mixer.render(mixed);
                    pcm_format.encode(mixed, pcm);
                    stream.write(pcm);
                    submitted_frames += count;
                    for (unsigned channel = 0; channel < voice_completions.size(); ++channel)
                        if (mixer.voices()[channel].rendered_complete && !voice_completions[channel])
                            voice_completions[channel] = submitted_frames;
                    ++submitted;
                    available -= count;
                }
                last_fill = now;
            };
            {
                std::lock_guard lock(mutex);
                buffer_frames = stream.capacity_frames();
                refill();
                stream.pause(false);
                opened = true;
            }
            condition.notify_all();
            const HANDLE events[] = {stop_event, wake, stream.ready()};
            for (;;) {
                const auto result = WaitForMultipleObjects(3, events, FALSE, connected ? INFINITE : 1000);
                if (result == WAIT_OBJECT_0)
                    return;
                if (result == WAIT_FAILED)
                    throw std::runtime_error("Audio output: cannot wait for completion");
                std::lock_guard lock(mutex);
                try {
                    if (stream.changed() || !connected) {
                        stream.reopen();
                        connected = true;
                        ++recoveries;
                        buffer_frames = stream.capacity_frames();
                        last_fill = std::chrono::steady_clock::now();
                    }
                    if (paused)
                        stream.pause(true);
                    else {
                        refill();
                        stream.pause(false);
                    }
                    effective_pause = paused;
                } catch (const std::exception& problem) {
                    // Keep mixer/session ownership while endpoints disappear.
                    error = problem.what();
                    connected = false;
                    effective_pause = paused;
                }
                condition.notify_all();
            }
        } catch (...) {
            std::lock_guard lock(mutex);
            failure = std::current_exception();
            opened = true;
            condition.notify_all();
        }
    }
    ~Impl() {
        if (worker.joinable()) {
            SetEvent(stop_event);
            worker.join();
        }
        CloseHandle(wake);
        CloseHandle(stop_event);
    }
};
SoundOutput::SoundOutput(const std::filesystem::path& assets, unsigned rate, unsigned channels, SoundPcmFormat format, SoundRequestFilter filter) :
    impl_(std::make_unique<Impl>(assets, rate, channels, format, filter)) {
    impl_->start();
}
SoundOutput::~SoundOutput() = default;
void SoundOutput::stop(int id, int channel, int group) {
    std::lock_guard lock(impl_->mutex);
    impl_->mixer.stop(id, channel, group);
    for (unsigned index = 0; index < impl_->voice_completions.size(); ++index)
        if (!impl_->mixer.voices()[index].status)
            impl_->voice_completions[index] = 0;
}
bool SoundOutput::active(int id, int channel, int group) const {
    std::lock_guard lock(impl_->mutex);
    return impl_->mixer.active(id, channel, group);
}
void SoundOutput::sound(SoundRequest request) {
    std::lock_guard lock(impl_->mutex);
    const auto channel = impl_->mixer.request(request);
    if (channel >= 0)
        impl_->voice_completions[unsigned(channel)] = 0;
}
void SoundOutput::pump() {
    std::lock_guard lock(impl_->mutex);
    impl_->rethrow_failure();
}
void SoundOutput::pause(bool paused) {
    std::unique_lock lock(impl_->mutex);
    impl_->rethrow_failure();
    if (paused == impl_->paused)
        return;
    impl_->paused = paused;
    SetEvent(impl_->wake);
    impl_->condition.wait(lock, [&] { return impl_->effective_pause == paused || impl_->failure; });
    impl_->rethrow_failure();
}
std::string SoundOutput::take_error() {
    std::lock_guard lock(impl_->mutex);
    return std::exchange(impl_->error, {});
}
unsigned SoundOutput::buffer_frames() const {
    return impl_->buffer_frames;
}
std::uint64_t SoundOutput::recoveries() const {
    return impl_->recoveries;
}
std::uint64_t SoundOutput::submitted_buffers() const {
    return impl_->submitted.load();
}
std::uint64_t SoundOutput::underruns() const {
    return impl_->underruns.load();
}
unsigned SoundOutput::rate() const {
    return impl_->bank.rate;
}
unsigned SoundOutput::channels() const {
    return unsigned(impl_->mixer.voices().size());
}
SoundPcmFormat SoundOutput::format() const {
    return impl_->pcm_format;
}
unsigned SoundOutput::queued_finished_voices() const {
    std::lock_guard lock(impl_->mutex);
    return unsigned(std::count_if(impl_->mixer.voices().begin(), impl_->mixer.voices().end(), [](const auto& voice) { return voice.rendered_complete; }));
}
void SoundOutput::volume(unsigned value) {
    std::lock_guard lock(impl_->mutex);
    impl_->rethrow_failure();
    impl_->mixer.volume(value);
}
} // namespace web
