#pragma once
#include "web.hpp"
#include <stdexcept>

namespace web {
// Named access to verified wgame.cfg fields. Unowned words survive edits.
class GameOptions {
public:
    GameOptions();
    explicit GameOptions(Configuration);
    const Configuration& configuration() const {
        return value_;
    }
    unsigned view() const;
    // Original camera menu selection is 1..6; view() returns the asset suffix
    // (0,1,2,4,5,6), in the original menu's different ordering.
    unsigned camera_selection() const {
        return value_[configuration_field::view];
    }
    void camera_selection(unsigned selection);
    unsigned screen_mode() const {
        return value_[configuration_field::screen_mode];
    }
    void screen_mode(unsigned);
    unsigned display_variant() const;
    void display_variant(unsigned);
    unsigned graphic_detail() const {
        return value_[configuration_field::graphic_detail];
    }
    void graphic_detail(unsigned);
    bool screen_flip() const {
        return value_[configuration_field::screen_flip] != 0;
    }
    void screen_flip(bool enabled) {
        value_[configuration_field::screen_flip] = enabled ? 1 : 0;
    }
    unsigned music_restart_policy() const {
        return value_[configuration_field::music_restart] - 1;
    }
    void music_restart_policy(unsigned policy) {
        if (policy > 2)
            throw std::out_of_range("Unknown sound transition policy");
        value_[configuration_field::music_restart] = policy + 1;
    }
    void view(unsigned);
    unsigned key(unsigned action) const;
    void key(unsigned action, unsigned key);
    void reset_controls(); // Original PLAY Defaults restores only the five keys.
    bool sound_enabled() const {
        return value_[configuration_field::sound_rate] != 0;
    }
    unsigned sound_rate() const {
        return value_[configuration_field::sound_rate];
    }
    unsigned sound_channels() const {
        return value_[configuration_field::sound_channels] < 3 ? value_[configuration_field::sound_channels] : 3; // Original device initializer 0x416fee.
    }
    bool visual_nudge() const {
        return value_[configuration_field::visual_nudge] != 0;
    }
    void visual_nudge(bool enabled) {
        value_[configuration_field::visual_nudge] = enabled;
    }
    void sound_rate(unsigned);
    bool sound_stereo() const {
        return value_[configuration_field::sound_stereo] != 0;
    }
    void sound_stereo(bool enabled) {
        value_[configuration_field::sound_stereo] = enabled;
    }
    bool sound_16bit() const {
        return value_[configuration_field::sound_16bit] != 0;
    }
    void sound_16bit(bool enabled) {
        value_[configuration_field::sound_16bit] = enabled;
    }
    void sound_enabled(bool enabled);
    unsigned volume() const {
        return value_[configuration_field::effects_volume];
    }
    void volume(unsigned);
    bool adjust_volume_key(unsigned key); // Original input poll: six arrows/numpad keys, +/-8.
    unsigned music_volume() const {
        return value_[configuration_field::music_volume];
    }
    bool music_enabled() const {
        return music_volume() != 0;
    }
    void music_volume(unsigned volume) {
        if (volume > 255)
            throw std::out_of_range("Music volume must be between 0 and 255");
        value_[configuration_field::music_volume] = volume;
    }
    std::uint32_t actions(const std::array<bool, 256>& held) const;
    static bool reserved_key(unsigned);
    static void validate(const Configuration&);

private:
    Configuration value_;
};
} // namespace web
