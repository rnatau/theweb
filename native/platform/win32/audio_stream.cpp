#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "audio_stream.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <sstream>

namespace web::windows {
namespace {
void check(HRESULT result) {
    if (FAILED(result)) {
        std::ostringstream text;
        text << "Audio output: WASAPI failure 0x" << std::hex << unsigned(result);
        throw std::runtime_error(text.str());
    }
}
} // namespace
class AudioStream::Notifications final : public IMMNotificationClient {
public:
    HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    std::atomic<bool> changed = false;
    Notifications() {
        if (!event)
            throw std::runtime_error("Audio output: cannot create endpoint event");
    }
    ~Notifications() {
        CloseHandle(event);
    }
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out)
            return E_POINTER;
        *out = nullptr;
        if (iid == __uuidof(IUnknown) || iid == __uuidof(IMMNotificationClient)) {
            *out = static_cast<IMMNotificationClient*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override {
        return ++references_;
    }
    ULONG STDMETHODCALLTYPE Release() override {
        const auto count = --references_;
        if (!count)
            delete this;
        return count;
    }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) override {
        if (flow == eRender && role == eConsole)
            signal();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override {
        signal();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override {
        signal();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override {
        signal();
        return S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override {
        return S_OK;
    }

private:
    std::atomic<ULONG> references_ = 1;
    void signal() {
        changed = true;
        SetEvent(event);
    }
};
AudioStream::AudioStream(unsigned rate, unsigned channels, unsigned bits) {
    format_ = {WAVE_FORMAT_PCM, WORD(channels), rate, rate * channels * bits / 8, WORD(channels * bits / 8), WORD(bits), 0};
    *notifications_.put() = new Notifications;
    check(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), reinterpret_cast<void**>(devices_.put())));
    check(devices_->RegisterEndpointNotificationCallback(notifications_.get()));
    try {
        open();
    } catch (...) {
        devices_->UnregisterEndpointNotificationCallback(notifications_.get());
        throw;
    }
}
AudioStream::~AudioStream() {
    if (devices_)
        devices_->UnregisterEndpointNotificationCallback(notifications_.get());
    if (client_)
        client_->Stop();
}
void AudioStream::open() {
    ComPtr<IMMDevice> endpoint;
    check(devices_->GetDefaultAudioEndpoint(eRender, eConsole, endpoint.put()));
    check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client_.put())));
    constexpr DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
    ComPtr<IAudioClient3> modern;
    HRESULT initialized = E_NOINTERFACE;
    if (SUCCEEDED(client_->QueryInterface(__uuidof(IAudioClient3), reinterpret_cast<void**>(modern.put())))) {
        UINT32 normal = 0, fundamental = 0, minimum = 0, maximum = 0;
        if (SUCCEEDED(modern->GetSharedModeEnginePeriod(&format_, &normal, &fundamental, &minimum, &maximum)) && fundamental) {
            // Negotiate a supported period without changing the source PCM format.
            const auto desired = std::max(minimum, UINT32((format_.nSamplesPerSec + 199) / 200));
            const auto period = std::min(maximum, (desired + fundamental - 1) / fundamental * fundamental);
            initialized = modern->InitializeSharedAudioStream(flags, period, &format_, nullptr);
        }
    }
    if (FAILED(initialized)) {
        modern.reset();
        check(endpoint->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void**>(client_.put())));
        check(client_->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, 100000, 0, &format_, nullptr));
    }
    check(client_->SetEventHandle(ready()));
    check(client_->GetBufferSize(&capacity_));
    check(client_->GetService(__uuidof(IAudioRenderClient), reinterpret_cast<void**>(render_.put())));
    written_ = 0;
}
HANDLE AudioStream::ready() const {
    return notifications_->event;
}
bool AudioStream::changed() {
    return notifications_->changed.exchange(false);
}
void AudioStream::reopen() {
    if (client_)
        client_->Stop();
    // The disconnected endpoint cannot play its remaining queued frames.
    base_ += written_;
    written_ = 0;
    render_.reset();
    client_.reset();
    open();
    paused_ = true;
}
unsigned AudioStream::available() {
    UINT32 padding = 0;
    check(client_->GetCurrentPadding(&padding));
    const auto free = capacity_ - std::min(capacity_, padding);
    // Maximum source frames whose exact rational expansion fits this packet.
    return free;
}
std::uint64_t AudioStream::played() {
    UINT32 padding = 0;
    check(client_->GetCurrentPadding(&padding));
    return base_ + written_ - std::min<std::uint64_t>(written_, padding);
}
void AudioStream::write(std::span<const std::uint8_t> pcm) {
    if (pcm.size() % format_.nBlockAlign)
        throw std::invalid_argument("Incomplete audio frame");
    const auto frames = UINT32(pcm.size() / format_.nBlockAlign);
    if (!frames)
        return;
    BYTE* buffer = nullptr;
    check(render_->GetBuffer(frames, &buffer));
    std::memcpy(buffer, pcm.data(), pcm.size());
    check(render_->ReleaseBuffer(frames, 0));
    written_ += frames;
}

void AudioStream::pause(bool value) {
    if (paused_ == value)
        return;
    check(value ? client_->Stop() : client_->Start());
    paused_ = value;
}
void AudioStream::reset() {
    pause(true);
    check(client_->Reset());
    base_ += written_;
    written_ = 0;
}
} // namespace web::windows
