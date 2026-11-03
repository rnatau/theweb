#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "audio_stream.hpp"
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include "music_output.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

namespace web {
namespace {
void check(HRESULT result, const char* operation) {
    if (FAILED(result)) {
        std::ostringstream message;
        message << "Music " << operation << " failed (0x" << std::hex << unsigned(result) << ')';
        throw std::runtime_error(message.str());
    }
}
template <class T> using Com = std::unique_ptr<T, void (*)(T*)>;
template <class T> Com<T> com(T* value) {
    return Com<T>(value, [](T* p) {
        if (p)
            p->Release();
    });
}
class Decoder {
public:
    explicit Decoder(const std::filesystem::path& file) {
        IMFSourceReader* raw = nullptr;
        check(MFCreateSourceReaderFromURL(file.c_str(), nullptr, &raw), "open MP3");
        reader_ = com(raw);
        check(reader_->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, FALSE), "deselect streams");
        check(reader_->SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE), "select audio");
        IMFMediaType* type = nullptr;
        check(MFCreateMediaType(&type), "create format");
        auto format = com(type);
        check(type->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio), "set audio type");
        check(type->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM), "set PCM");
        check(type->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 2), "set stereo");
        check(type->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, 44100), "set rate");
        check(type->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16), "set sample size");
        check(type->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 4), "set alignment");
        check(type->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 176400), "set byte rate");
        check(reader_->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, type), "decode stereo PCM");
    }
    bool next(std::vector<std::int16_t>& samples) {
        samples.clear();
        while (!ended_) {
            DWORD flags = 0;
            LONGLONG timestamp = 0;
            IMFSample* raw = nullptr;
            check(reader_->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nullptr, &flags, &timestamp, &raw), "read MP3");
            auto sample = com(raw);
            if (flags & MF_SOURCE_READERF_ERROR)
                throw std::runtime_error("Music decoder reported a stream error");
            if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
                throw std::runtime_error("Music format changed while playing");
            ended_ = (flags & MF_SOURCE_READERF_ENDOFSTREAM) != 0;
            if (!sample)
                continue;
            IMFMediaBuffer* buffer = nullptr;
            check(sample->ConvertToContiguousBuffer(&buffer), "read PCM buffer");
            auto owned = com(buffer);
            BYTE* bytes = nullptr;
            DWORD length = 0;
            check(buffer->Lock(&bytes, nullptr, &length), "lock PCM");
            if (length % 4) {
                buffer->Unlock();
                throw std::runtime_error("Incomplete stereo music frame");
            }
            try {
                samples.resize(length / 2);
                std::memcpy(samples.data(), bytes, length);
            } catch (...) {
                buffer->Unlock();
                throw;
            }
            check(buffer->Unlock(), "unlock PCM");
            if (!samples.empty())
                return true;
        }
        return false;
    }

private:
    Com<IMFSourceReader> reader_ = com<IMFSourceReader>(nullptr);
    bool ended_ = false;
};
} // namespace
class MusicOutput::Impl {
public:
    MusicCatalog catalog;
    std::mutex mutex;
    MusicPlayback requested;
    unsigned volume = 0;
    bool paused = true;
    std::optional<MusicPlayback> completion;
    std::string error;
    HANDLE wake = nullptr, stop = nullptr;
    std::thread worker;
    std::atomic<unsigned> physical = 0;
    std::atomic<std::uint64_t> submitted = 0, frames = 0, starts = 0;
    // Owned-process regression runs decode/queue real music without speaker output.
    const bool silent_test = GetEnvironmentVariableW(L"THEWEB_TEST_SILENT_MUSIC", nullptr, 0) != 0;
    explicit Impl(MusicCatalog value) :
        catalog(std::move(value)) {
        wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (!wake || !stop) {
            close_events();
            throw std::runtime_error("Cannot create music events");
        }
        try {
            worker = std::thread([this] { run(); });
        } catch (...) {
            close_events();
            throw;
        }
    }
    ~Impl() {
        SetEvent(stop);
        worker.join();
        close_events();
    }
    void close_events() {
        if (wake)
            CloseHandle(wake);
        if (stop)
            CloseHandle(stop);
    }
    void run() noexcept {
        try {
            windows::ComApartment apartment;
            check(MFStartup(MF_VERSION), "initialize decoder");
            struct MediaFoundation {
                ~MediaFoundation() {
                    MFShutdown();
                }
            } runtime;
            windows::AudioStream output(44100, 2, 16);
            std::array<std::int16_t, 1024> pcm{};
            std::unique_ptr<Decoder> decoder;
            std::vector<std::int16_t> decoded;
            std::size_t position = 0;
            MusicPlayback current;
            bool have_current = false, ended = true, notified = false, failed = false, connected = true;
            unsigned cd = 0, last = 0;
            std::uint64_t final_frame = 0;
            const HANDLE events[] = {stop, wake, output.ready()};
            for (;;) {
                const auto result = WaitForMultipleObjects(3, events, FALSE, connected ? INFINITE : 1000);
                if (result == WAIT_OBJECT_0)
                    break;
                if (result == WAIT_FAILED)
                    throw std::runtime_error("Cannot wait for music completion");
                MusicPlayback target;
                unsigned level;
                bool suspended;
                {
                    std::lock_guard lock(mutex);
                    target = requested;
                    level = volume;
                    suspended = paused;
                }
                try {
                    if (output.changed() || !connected) {
                        output.reopen();
                        connected = true;
                    }
                    const bool changed = !have_current || target.track != current.track || target.revision != current.revision;
                    if (changed) {
                        output.reset();
                        decoder.reset();
                        decoded.clear();
                        position = 0;
                        current = target;
                        have_current = true;
                        notified = failed = false;
                        const auto range = MusicCatalog::range(target.track);
                        if (range)
                            ++starts;
                        cd = range ? range->first : 0;
                        last = range ? range->last : 0;
                        physical = cd;
                        ended = !range;
                        std::lock_guard lock(mutex);
                        completion.reset();
                    }
                    if (suspended)
                        output.pause(true);
                    if (!suspended && !ended) {
                        auto available = output.available();
                        while (available && !ended) {
                            const auto limit = std::min<std::size_t>(available * 2, pcm.size());
                            std::size_t count = 0;
                            while (count < limit && !ended) {
                                if (position == decoded.size()) {
                                    if (!decoder)
                                        decoder = std::make_unique<Decoder>(catalog.file(cd));
                                    if (!decoder->next(decoded)) {
                                        position = 0;
                                        decoder.reset();
                                        if (cd == last) {
                                            ended = true;
                                            break;
                                        }
                                        ++cd;
                                        physical = cd;
                                        continue;
                                    }
                                    position = 0;
                                }
                                const auto size = std::min(decoded.size() - position, limit - count);
                                std::copy_n(decoded.data() + position, size, pcm.data() + count);
                                count += size;
                                position += size;
                            }
                            if (!count)
                                break;
                            // Original fade timer delays changes; it does not attenuate PCM.
                            const auto gain = silent_test ? 0.0 : double(level) / 255.0;
                            for (std::size_t i = 0; i < count; ++i)
                                pcm[i] = std::int16_t(std::lround(pcm[i] * gain));
                            output.write({reinterpret_cast<const std::uint8_t*>(pcm.data()), count * sizeof(pcm[0])});
                            ++submitted;
                            frames += count / 2;
                            final_frame = frames;
                            available -= unsigned(count / 2);
                        }
                    }
                    if (!suspended && output.played() < final_frame)
                        output.pause(false);
                    if (ended && output.played() >= final_frame && !notified && !failed && MusicCatalog::range(current.track)) {
                        notified = true;
                        physical = 0;
                        output.pause(true);
                        std::lock_guard lock(mutex);
                        if (requested.track == current.track && requested.revision == current.revision)
                            completion = current;
                    }
                } catch (const std::exception& problem) {
                    std::lock_guard lock(mutex);
                    error = problem.what();
                    // A missing media file completes this attempt without retrying it.
                    // Endpoint failures retain the decoder and retry device ownership.
                    if (std::string_view(problem.what()).starts_with("Audio output:"))
                        connected = false;
                    else {
                        decoder.reset();
                        ended = failed = true;
                        physical = 0;
                    }
                }
            }
        } catch (const std::exception& problem) {
            std::lock_guard lock(mutex);
            error = problem.what();
            physical = 0;
        }
    }
};
MusicOutput::MusicOutput(MusicCatalog catalog) :
    impl_(std::make_unique<Impl>(std::move(catalog))) {}
MusicOutput::~MusicOutput() = default;
void MusicOutput::update(MusicPlayback playback, unsigned volume) {
    if (volume > 255)
        throw std::out_of_range("Music volume must be between 0 and 255");
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->requested.track == playback.track && impl_->requested.revision == playback.revision && impl_->volume == volume)
            return;
        impl_->requested = playback;
        impl_->volume = volume;
    }
    SetEvent(impl_->wake);
}
void MusicOutput::pause(bool paused) {
    {
        std::lock_guard lock(impl_->mutex);
        if (impl_->paused == paused)
            return;
        impl_->paused = paused;
    }
    SetEvent(impl_->wake);
}
std::optional<MusicPlayback> MusicOutput::completed() {
    std::lock_guard lock(impl_->mutex);
    auto result = impl_->completion;
    impl_->completion.reset();
    return result;
}
std::string MusicOutput::take_error() {
    std::lock_guard lock(impl_->mutex);
    auto result = std::move(impl_->error);
    impl_->error.clear();
    return result;
}
unsigned MusicOutput::cd_track() const {
    return impl_->physical.load();
}
std::uint64_t MusicOutput::submitted_buffers() const {
    return impl_->submitted.load();
}
std::uint64_t MusicOutput::submitted_frames() const {
    return impl_->frames.load();
}
std::uint64_t MusicOutput::playback_starts() const {
    return impl_->starts.load();
}
} // namespace web
