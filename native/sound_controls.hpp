#pragma once
#include <cstdint>

namespace web {
// Original logarithmic DirectSound control tables. PCM/device quantization is
// owned by the replacement mixer/backend, rather than claimed OS equivalence.
class SoundControls {
public:
    static int master_decibels(unsigned volume);
    static int attenuation_decibels(unsigned attenuation);
    static int pan_decibels(int pan, std::uint16_t extra_sample_step = 0);
    static double gain(int hundredths_db);
};
} // namespace web
