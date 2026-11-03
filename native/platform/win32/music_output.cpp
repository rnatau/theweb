#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
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
void audio_check(MMRESULT result) {
    if (result != MMSYSERR_NOERROR)
        throw std::runtime_error("Music audio device failed (" + std::to_string(result) + ')');
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
    HANDLE wake = nullptr, stop = nullptr, done = nullptr;
    std::thread worker;
    std::atomic<unsigned> physical = 0;
    std::atomic<std::uint64_t> submitted = 0, frames = 0, starts = 0;
    // Owned-process regression runs decode/queue real music without speaker output.
    const bool silent_test = GetEnvironmentVariableW(L"THEWEB_TEST_SILENT_MUSIC", nullptr, 0) != 0;
    explicit Impl(MusicCatalog value) :
        catalog(std::move(value)) {
        wake = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
        done = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!wake || !stop || !done) {
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
        if (done)
            CloseHandle(done);
    }
    void run() noexcept {
        HWAVEOUT device = nullptr;
        struct Buffer {
            std::array<std::int16_t, 4096> pcm{};
            WAVEHDR header{};
            bool prepared = false;
        };
        std::array<Buffer, 4> buffers;
        bool com_started = false, mf_started = false;
        try {
            check(CoInitializeEx(nullptr, COINIT_MULTITHREADED), "initialize COM");
            com_started = true;
            check(MFStartup(MF_VERSION), "initialize decoder");
            mf_started = true;
            WAVEFORMATEX format{WAVE_FORMAT_PCM, 2, 44100, 176400, 4, 16, 0};
            audio_check(waveOutOpen(&device, WAVE_MAPPER, &format, DWORD_PTR(done), 0, CALLBACK_EVENT));
            audio_check(waveOutPause(device));
            for (auto& buffer : buffers) {
                buffer.header.lpData = reinterpret_cast<char*>(buffer.pcm.data());
                buffer.header.dwBufferLength = DWORD(buffer.pcm.size() * 2);
                audio_check(waveOutPrepareHeader(device, &buffer.header, sizeof(WAVEHDR)));
                buffer.prepared = true;
            }
            std::unique_ptr<Decoder> decoder;
            std::vector<std::int16_t> decoded;
            std::size_t position = 0;
            MusicPlayback current;
            bool have_current = false, ended = true, notified = false, failed = false, output_paused = true;
            unsigned cd = 0, last = 0;
            const HANDLE events[] = {stop, wake, done};
            for (;;) {
                const auto result = WaitForMultipleObjects(3, events, FALSE, INFINITE);
                if (result == WAIT_OBJECT_0)
                    break;
                if (result != WAIT_OBJECT_0 + 1 && result != WAIT_OBJECT_0 + 2)
                    throw std::runtime_error("Cannot wait for music playback");
                MusicPlayback target;
                unsigned level;
                bool suspended;
                {
                    std::lock_guard lock(mutex);
                    target = requested;
                    level = volume;
                    suspended = paused;
                }
                const bool changed = !have_current || target.track != current.track || target.revision != current.revision;
                if (changed) {
                    audio_check(waveOutPause(device));
                    audio_check(waveOutReset(device));
                    audio_check(waveOutPause(device));
                    output_paused = true;
                    decoder.reset();
                    decoded.clear();
                    position = 0;
                    current = target;
                    have_current = true;
                    notified = false;
                    failed = false;
                    const auto range = MusicCatalog::range(target.track);
                    if (range)
                        ++starts;
                    cd = range ? range->first : 0;
                    last = range ? range->last : 0;
                    physical = cd;
                    ended = !range;
                    {
                        std::lock_guard lock(mutex);
                        completion.reset();
                    }
                }
                if (suspended && !output_paused) {
                    audio_check(waveOutPause(device));
                    output_paused = true;
                }
                if (!suspended && !ended) {
                    try {
                        for (auto& buffer : buffers) {
                            if (buffer.header.dwFlags & WHDR_INQUEUE)
                                continue;
                            std::size_t count = 0;
                            while (count < buffer.pcm.size() && !ended) {
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
                                const auto size = std::min(decoded.size() - position, buffer.pcm.size() - count);
                                std::copy_n(decoded.data() + position, size, buffer.pcm.data() + count);
                                count += size;
                                position += size;
                            }
                            if (!count)
                                break;
                            // Windows 1.50P's 0x4155c0 is RET. The shared timer
                            // delays a track change; it does not attenuate PCM.
                            const auto gain = silent_test ? 0.0 : double(level) / 255.0;
                            for (std::size_t i = 0; i < count; ++i)
                                buffer.pcm[i] = std::int16_t(std::lround(buffer.pcm[i] * gain));
                            buffer.header.dwBufferLength = DWORD(count * 2);
                            audio_check(waveOutWrite(device, &buffer.header, sizeof(WAVEHDR)));
                            ++submitted;
                            frames += count / 2;
                        }
                    } catch (const std::exception& problem) {
                        decoder.reset();
                        ended = true;
                        failed = true;
                        physical = 0;
                        std::lock_guard lock(mutex);
                        error = problem.what();
                    }
                }
                bool queued = false;
                for (const auto& buffer : buffers)
                    queued |= (buffer.header.dwFlags & WHDR_INQUEUE) != 0;
                if (!suspended && queued && output_paused) {
                    audio_check(waveOutRestart(device));
                    output_paused = false;
                }
                if (ended && !queued && !notified && !failed && MusicCatalog::range(current.track)) {
                    notified = true;
                    physical = 0;
                    std::lock_guard lock(mutex);
                    if (requested.track == current.track && requested.revision == current.revision)
                        completion = current;
                }
            }
        } catch (const std::exception& problem) {
            std::lock_guard lock(mutex);
            error = problem.what();
            physical = 0;
        }
        if (device) {
            waveOutReset(device);
            for (auto& buffer : buffers)
                if (buffer.prepared)
                    waveOutUnprepareHeader(device, &buffer.header, sizeof(WAVEHDR));
            waveOutClose(device);
        }
        if (mf_started)
            MFShutdown();
        if (com_started)
            CoUninitialize();
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
        impl_->requested = playback;
        impl_->volume = volume;
    }
    SetEvent(impl_->wake);
}
void MusicOutput::pause(bool paused) {
    {
        std::lock_guard lock(impl_->mutex);
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
