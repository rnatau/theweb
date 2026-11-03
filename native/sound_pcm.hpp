#pragma once
#include "web.hpp"

namespace web {
// Modern device boundary: original primary-format requests, separate from the
// recovered PCM16 mono assets and mixer. Legacy DirectSound quantization is an
// OS boundary, so this uses explicit standard PCM conversion instead.
struct SoundPcmFormat {
    bool stereo = true, sixteen_bit = true;
    unsigned channels() const {
        return stereo ? 2 : 1;
    }
    unsigned bits() const {
        return sixteen_bit ? 16 : 8;
    }
    void encode(std::span<const std::int16_t> mixed, std::span<std::uint8_t> output) const {
        if (mixed.size() % 2 || output.size() != mixed.size() / 2 * channels() * bits() / 8)
            throw std::invalid_argument("Incomplete PCM output frames");
        std::size_t offset = 0;
        const auto sample = [&](int value) {
            if (sixteen_bit) {
                output[offset++] = std::uint8_t(value);
                output[offset++] = std::uint8_t(unsigned(value) >> 8);
            } else
                output[offset++] = std::uint8_t((value + 32768) / 256);
        };
        for (std::size_t frame = 0; frame < mixed.size(); frame += 2) {
            if (stereo) {
                sample(mixed[frame]);
                sample(mixed[frame + 1]);
            } else
                sample((int(mixed[frame]) + int(mixed[frame + 1])) / 2);
        }
    }
};
} // namespace web
