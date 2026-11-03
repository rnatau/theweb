#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#include "sound_output.hpp"
#include <algorithm>
#include <atomic>
#include <exception>
#include <mutex>
#include <thread>

namespace web {
namespace {
void check_audio(MMRESULT result) {
    if (result != MMSYSERR_NOERROR) {
        char message[MAXERRORLENGTH]{};
        waveOutGetErrorTextA(result, message, MAXERRORLENGTH);
        throw std::runtime_error(std::string("Audio output: ") + message);
    }
}
} // namespace
class SoundOutput::Impl {
public:
    SoundBank bank;
    SoundMixer mixer;
    SoundPcmFormat pcm_format;
    HWAVEOUT device = nullptr;
    struct Buffer {
        std::vector<std::int16_t> mixed;
        Bytes pcm;
        std::uint64_t end_frame = 0;
        WAVEHDR header{};
        bool prepared = false;
    };
    // Refill on device completion, independently of game/rendering deadlines.
    // Four blocks retain a short queue while tolerating normal device batching.
    std::array<Buffer, 4> buffers;
    bool paused = false;
    std::atomic<std::uint64_t> submitted = 0, underruns = 0;
    HANDLE completed_event = nullptr, stop_event = nullptr;
    std::mutex mutex;
    std::thread worker;
    std::exception_ptr failure;
    std::uint64_t submitted_frames = 0, played_frames = 0;
    std::vector<std::uint64_t> voice_completions;
    explicit Impl(const std::filesystem::path& assets, unsigned rate, unsigned channels, SoundPcmFormat format, SoundRequestFilter filter) :
        bank(SoundBank::load(assets, rate)),
        mixer(bank, std::min(channels, 3u), filter),
        pcm_format(format),
        voice_completions(mixer.voices().size()) {
        mixer.retain_until_played = true;
    }
    void open() {
        completed_event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!completed_event || !stop_event)
            throw std::runtime_error("Cannot create audio completion events");
        WAVEFORMATEX format{};
        format.wFormatTag = WAVE_FORMAT_PCM;
        format.nChannels = WORD(pcm_format.channels());
        format.nSamplesPerSec = bank.rate;
        format.wBitsPerSample = WORD(pcm_format.bits());
        format.nBlockAlign = WORD(format.nChannels * format.wBitsPerSample / 8);
        format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;
        check_audio(waveOutOpen(&device, WAVE_MAPPER, &format, DWORD_PTR(completed_event), 0, CALLBACK_EVENT));
        for (auto& buffer : buffers) {
            // Retain approximately the same queue duration at each original rate.
            constexpr unsigned reference_rate = 44100, reference_block_frames = 512, block_alignment = 8;
            const auto frames = ((bank.rate * reference_block_frames + reference_rate - 1) / reference_rate + block_alignment - 1) / block_alignment * block_alignment;
            buffer.mixed.resize(frames * 2);
            buffer.pcm.resize(frames * format.nBlockAlign);
            buffer.header.lpData = reinterpret_cast<char*>(buffer.pcm.data());
            buffer.header.dwBufferLength = DWORD(buffer.pcm.size() * sizeof(buffer.pcm[0]));
            check_audio(waveOutPrepareHeader(device, &buffer.header, sizeof(WAVEHDR)));
            buffer.prepared = true;
        }
        // Prime the complete queue before playback starts.
        check_audio(waveOutPause(device));
        refill();
        check_audio(waveOutRestart(device));
        worker = std::thread([this] { run(); });
    }
    void rethrow_failure() const {
        if (failure)
            std::rethrow_exception(failure);
    }
    void refill() {
        unsigned queued = 0;
        for (const auto& buffer : buffers) {
            queued += (buffer.header.dwFlags & WHDR_INQUEUE) != 0;
            if (!(buffer.header.dwFlags & WHDR_INQUEUE))
                played_frames = std::max(played_frames, buffer.end_frame);
        }
        for (unsigned channel = 0; channel < voice_completions.size(); ++channel)
            if (voice_completions[channel] && voice_completions[channel] <= played_frames) {
                mixer.complete(channel);
                voice_completions[channel] = 0;
            }
        if (!queued && submitted)
            ++underruns;
        for (auto& buffer : buffers)
            if (!(buffer.header.dwFlags & WHDR_INQUEUE)) {
                mixer.render(buffer.mixed);
                pcm_format.encode(buffer.mixed, buffer.pcm);
                buffer.end_frame = submitted_frames + buffer.mixed.size() / 2;
                for (unsigned channel = 0; channel < voice_completions.size(); ++channel)
                    if (mixer.voices()[channel].rendered_complete && !voice_completions[channel])
                        voice_completions[channel] = buffer.end_frame;
                check_audio(waveOutWrite(device, &buffer.header, sizeof(WAVEHDR)));
                submitted_frames = buffer.end_frame;
                ++submitted;
            }
    }
    void run() noexcept {
        try {
            const HANDLE events[] = {stop_event, completed_event};
            for (;;) {
                const auto result = WaitForMultipleObjects(2, events, FALSE, INFINITE);
                if (result == WAIT_OBJECT_0)
                    return;
                if (result != WAIT_OBJECT_0 + 1)
                    throw std::runtime_error("Cannot wait for audio completion");
                std::lock_guard lock(mutex);
                if (!paused)
                    refill();
            }
        } catch (...) {
            std::lock_guard lock(mutex);
            failure = std::current_exception();
        }
    }
    ~Impl() {
        if (worker.joinable()) {
            SetEvent(stop_event);
            worker.join();
        }
        if (device) {
            waveOutReset(device);
            for (auto& buffer : buffers)
                if (buffer.prepared)
                    waveOutUnprepareHeader(device, &buffer.header, sizeof(WAVEHDR));
            waveOutClose(device);
        }
        if (completed_event)
            CloseHandle(completed_event);
        if (stop_event)
            CloseHandle(stop_event);
    }
};
SoundOutput::SoundOutput(const std::filesystem::path& assets, unsigned rate, unsigned channels, SoundPcmFormat format, SoundRequestFilter filter) :
    impl_(std::make_unique<Impl>(assets, rate, channels, format, filter)) {
    impl_->open();
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
    std::lock_guard lock(impl_->mutex);
    impl_->rethrow_failure();
    if (paused == impl_->paused)
        return;
    if (!paused)
        impl_->refill();
    check_audio(paused ? waveOutPause(impl_->device) : waveOutRestart(impl_->device));
    impl_->paused = paused;
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
std::string SoundOutput::take_error() {
    return {};
}
unsigned SoundOutput::buffer_frames() const {
    return unsigned(impl_->buffers.size() * impl_->buffers.front().mixed.size() / 2);
}
std::uint64_t SoundOutput::recoveries() const {
    return 0;
}
} // namespace web
