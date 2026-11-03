#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../platform/win32/audio_stream.hpp"
#include <iostream>
#include <vector>

int main() {
    web::windows::ComApartment apartment;
    web::windows::AudioStream stream(44100, 2, 16);
    const auto capacity = stream.capacity_frames();
    if (!capacity || capacity > 44100)
        throw std::runtime_error("Invalid negotiated audio buffer");
    const auto fill = [&] {
        std::vector<std::uint8_t> silence(stream.available() * 4);
        if (!silence.empty())
            stream.write(silence);
        stream.pause(false);
    };
    fill();
    if (WaitForSingleObject(stream.ready(), 1000) != WAIT_OBJECT_0)
        throw std::runtime_error("WASAPI completion did not signal");
    stream.pause(true);
    const auto before = stream.played();
    stream.reopen();
    if (stream.played() < before)
        throw std::runtime_error("Reopened endpoint moved the playback clock backwards");
    fill();
    if (WaitForSingleObject(stream.ready(), 1000) != WAIT_OBJECT_0)
        throw std::runtime_error("Reopened endpoint did not play");
    stream.reset();
    fill();
    stream.pause(true);
    std::cout << "PASS: WASAPI completion, pause, endpoint recreation and reset; negotiated queue " << capacity << " frames (" << capacity * 1000.0 / 44100 << " ms)\n";
}
