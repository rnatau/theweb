#include "sound_controls.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace web {
int SoundControls::master_decibels(unsigned volume) {
    if (volume > 127)
        throw std::out_of_range("Effects volume must be between 0 and 127");
    // Original x87 conversion at 0x417288..0x4172ca truncates toward zero.
    return volume ? int(2000.0 * std::log10(double(volume) / 127.0)) : -10000;
}
int SoundControls::attenuation_decibels(unsigned attenuation) {
    if (attenuation > 127)
        throw std::out_of_range("Effects attenuation must be between 0 and 127");
    return master_decibels(127 - attenuation);
}
int SoundControls::pan_decibels(int pan, std::uint16_t extra_sample_step) {
    pan = std::clamp(pan, -128, 127);
    // The original -128 lookup precedes the 255-word pan table and aliases
    // sample183's initial step at 0x43418e. Preserve the result safely.
    if (pan == -128)
        return std::int16_t(extra_sample_step);
    return pan * 1250 / 128;
}
double SoundControls::gain(int hundredths_db) {
    // DirectSound's minimum control value denotes silence. The native backend
    // uses exact zero at that endpoint, rather than retaining inaudible residue.
    return hundredths_db <= -10000 ? 0.0 : std::pow(10.0, double(hundredths_db) / 2000.0);
}
} // namespace web
