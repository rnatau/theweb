#include "options.hpp"
#include <algorithm>

namespace web {
namespace {
constexpr std::array<unsigned, 6> original_views = {2, 1, 0, 6, 5, 4};
constexpr std::array<unsigned, 6> original_modes = {0x10, 0x20, 0x30, 0x50, 0x60, 0x70};
constexpr std::array<std::uint32_t, 5> action_bits = {input_action::left_flipper, input_action::right_flipper, input_action::nudge_negative, input_action::nudge_positive, input_action::nudge_up};
} // namespace
GameOptions::GameOptions() :
    value_(default_configuration()) {
    view(0);        // Retain this native host's default camera.
    screen_mode(6); // New native settings retain maximum-quality assets.
    graphic_detail(3);
}
GameOptions::GameOptions(Configuration value) :
    value_(value) {
    validate(value_);
}
unsigned GameOptions::view() const {
    return original_views.at(value_[configuration_field::view] - 1);
}
void GameOptions::camera_selection(unsigned selection) {
    auto changed = value_;
    changed[configuration_field::view] = selection;
    validate(changed);
    value_ = changed;
}
void GameOptions::screen_mode(unsigned mode) {
    if (mode < 1 || mode > original_modes.size())
        throw std::out_of_range("Unknown original screen mode");
    value_[configuration_field::screen_mode] = mode;
}
unsigned GameOptions::display_variant() const {
    return original_modes.at(screen_mode() - 1) | view();
}
void GameOptions::display_variant(unsigned variant) {
    display_mode(variant);
    const auto mode = std::find(original_modes.begin(), original_modes.end(), variant & 0xf0);
    screen_mode(unsigned(mode - original_modes.begin()) + 1);
    view(variant & 15);
}
void GameOptions::graphic_detail(unsigned detail) {
    if (detail < 1 || detail > 3)
        throw std::out_of_range("Unknown original graphic detail");
    value_[configuration_field::graphic_detail] = detail;
}
void GameOptions::view(unsigned view) {
    const auto found = std::find(original_views.begin(), original_views.end(), view);
    if (found == original_views.end())
        throw std::invalid_argument("Unsupported table view");
    value_[configuration_field::view] = unsigned(found - original_views.begin()) + 1;
}
unsigned GameOptions::key(unsigned action) const {
    if (action >= action_bits.size())
        throw std::out_of_range("Unknown game control");
    return value_[configuration_field::first_key + action];
}
bool GameOptions::reserved_key(unsigned key) {
    // Original PLAY 0x402320 reserves S/P/C/Enter/arrows. The other entries
    // protect Windows navigation and this host's fixed commands.
    constexpr std::array<unsigned, 22> reserved = {
        'S', 'P',          'C',          key_code::enter, key_code::left,  key_code::right, key_code::up,           key_code::down,          key_code::tab,  key_code::escape, 'D',
        'V', key_code::f1, key_code::f2, key_code::f3,    key_code::pause, key_code::alt,   key_code::left_windows, key_code::right_windows, key_code::menu, key_code::f4,     key_code::f10};
    return key < 8 || key >= 255 || std::find(reserved.begin(), reserved.end(), key) != reserved.end();
}
void GameOptions::validate(const Configuration& value) {
    if (value[configuration_field::screen_mode] < 1 || value[configuration_field::screen_mode] > original_modes.size())
        throw std::runtime_error("Configuration has an invalid screen mode");
    if (value[configuration_field::graphic_detail] < 1 || value[configuration_field::graphic_detail] > 3)
        throw std::runtime_error("Configuration has an invalid graphic detail");
    if (value[configuration_field::view] < 1 || value[configuration_field::view] > original_views.size() || value[configuration_field::effects_volume] > 127)
        throw std::runtime_error("Configuration has an invalid view or effects volume");
    if (value[configuration_field::music_restart] < 1 || value[configuration_field::music_restart] > 3)
        throw std::runtime_error("Configuration has an invalid music restart policy");
    if (value[configuration_field::music_volume] > 255)
        throw std::runtime_error("Configuration has an invalid music volume");
    for (unsigned i = configuration_field::first_key; i < configuration_field::first_key + action_bits.size(); ++i) {
        if (reserved_key(value[i]))
            throw std::runtime_error("That key is reserved for a fixed command");
        for (unsigned j = configuration_field::first_key; j < i; ++j)
            if (value[j] == value[i])
                throw std::runtime_error("Each game control needs a different key");
    }
    if (value[configuration_field::sound_rate] != 0 && value[configuration_field::sound_rate] != 6000 && value[configuration_field::sound_rate] != 11025 &&
        value[configuration_field::sound_rate] != 22050 && value[configuration_field::sound_rate] != 44100)
        throw std::runtime_error("Configuration has an unsupported sample rate");
}
void GameOptions::key(unsigned action, unsigned key) {
    this->key(action);
    auto changed = value_;
    changed[configuration_field::first_key + action] = key;
    validate(changed);
    value_ = changed;
}
void GameOptions::reset_controls() {
    const auto defaults = default_configuration();
    std::copy_n(defaults.begin() + configuration_field::first_key, action_bits.size(), value_.begin() + configuration_field::first_key);
}
void GameOptions::sound_enabled(bool enabled) {
    if (!enabled)
        value_[configuration_field::sound_rate] = 0;
    else if (!value_[configuration_field::sound_rate])
        value_[configuration_field::sound_rate] = 22050;
}
void GameOptions::sound_rate(unsigned rate) {
    auto changed = value_;
    changed[configuration_field::sound_rate] = rate;
    validate(changed);
    value_ = changed;
}
void GameOptions::volume(unsigned value) {
    if (value > 127)
        throw std::out_of_range("Effects volume must be between 0 and 127");
    value_[configuration_field::effects_volume] = value;
}
bool GameOptions::adjust_volume_key(unsigned key) {
    int delta = 0;
    if (key == key_code::left || key == key_code::down || key == key_code::numpad_2)
        delta = -8;
    else if (key == key_code::right || key == key_code::up || key == key_code::numpad_8)
        delta = 8;
    if (!delta)
        return false;
    if (sound_enabled())
        volume(unsigned(std::clamp(int(volume()) + delta, 0, 127)));
    return true; // Disabled effects still request the original meter.
}
std::uint32_t GameOptions::actions(const std::array<bool, 256>& held) const {
    std::uint32_t result = 0;
    for (unsigned i = 0; i < action_bits.size(); ++i)
        if (held[key(i)])
            result |= action_bits[i];
    if (held[key_code::enter])
        result |= input_action::launch;
    if (held['S'] || held[key_code::f1])
        result |= input_action::start;
    if (held['C'])
        result |= input_action::continue_game;
    return result;
}
} // namespace web
