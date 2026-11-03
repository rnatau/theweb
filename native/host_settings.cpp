#include "host_settings.hpp"
#include <algorithm>
#include <bit>

namespace web {
std::uint64_t gamepad_buttons(GamepadSample sample, std::uint64_t previous) {
    auto result = sample.buttons;
    const auto axis = [&](int value, std::uint64_t bit) {
        if (value >= ((previous & bit) ? 10000 : 16000))
            result |= bit;
    };
    axis(-sample.x, pad_button::stick_left);
    axis(sample.x, pad_button::stick_right);
    axis(sample.y, pad_button::stick_up);
    axis(-sample.y, pad_button::stick_down);
    if (sample.left_trigger >= ((previous & pad_button::lt) ? 60 : 100))
        result |= pad_button::lt;
    if (sample.right_trigger >= ((previous & pad_button::rt) ? 60 : 100))
        result |= pad_button::rt;
    return result;
}
namespace {
constexpr std::array<std::uint32_t, HostSettings::control_count> actions = {
    input_action::left_flipper,
    input_action::right_flipper,
    input_action::nudge_negative,
    input_action::nudge_positive,
    input_action::nudge_up,
    input_action::launch,
    input_action::start,
    input_action::continue_game,
    host_action::pause,
    host_action::view,
    host_action::menu};
}
void HostSettings::validate() const {
    for (unsigned i = 0; i < keys.size(); ++i) {
        const auto key = keys[i];
        if (key < 8 || key >= 255 || key == key_code::alt || key == 164 || key == 165 || key == key_code::left_windows || key == key_code::right_windows || key == key_code::menu ||
            key == key_code::f3 || key == 'D' || (key == key_code::escape && i != 10))
            throw std::invalid_argument("RESERVED KEY");
        if (buttons[i] & ~pad_button::all)
            throw std::invalid_argument("INVALID BUTTON");
        for (unsigned j = 0; j < i; ++j) {
            if (key == keys[j])
                throw std::invalid_argument("KEY IN USE");
            if (buttons[i] & buttons[j])
                throw std::invalid_argument("BUTTON IN USE");
        }
    }
}
void HostSettings::bind_key(unsigned control, unsigned key) {
    auto draft = *this;
    draft.keys.at(control) = key;
    draft.validate();
    *this = draft;
}
void HostSettings::bind_button(unsigned control, std::uint64_t button) {
    if (!std::has_single_bit(button))
        throw std::invalid_argument("ONE BUTTON PLEASE");
    auto draft = *this;
    draft.buttons.at(control) = button;
    draft.validate();
    *this = draft;
}
std::uint32_t HostSettings::keyboard_actions(const std::array<bool, 256>& held) const {
    std::uint32_t result = 0;
    for (unsigned i = 0; i < keys.size(); ++i)
        if (held[keys[i]])
            result |= actions[i];
    return result;
}
std::uint32_t HostSettings::gamepad_actions(std::uint64_t held) const {
    std::uint32_t result = 0;
    if (gamepad)
        for (unsigned i = 0; i < buttons.size(); ++i)
            if (held & buttons[i])
                result |= actions[i];
    return result;
}
Bytes HostSettings::serialize() const {
    validate();
    Bytes bytes;
    const auto word = [&](std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            bytes.push_back(std::uint8_t(value >> (i * 8)));
    };
    word(0x31495557); // WUI1
    word(control_count);
    word(unsigned(fullscreen) | (unsigned(gamepad) << 1));
    for (auto key : keys)
        word(key);
    for (auto button : buttons) {
        word(std::uint32_t(button));
        word(std::uint32_t(button >> 32));
    }
    return bytes;
}
HostSettings HostSettings::parse(std::span<const std::uint8_t> bytes) {
    if (bytes.size() != record_bytes || u32(bytes, 0) != 0x31495557 || u32(bytes, 4) != control_count || u32(bytes, 8) > 3)
        throw std::runtime_error("Invalid host settings format");
    HostSettings value;
    value.fullscreen = u32(bytes, 8) & 1;
    value.gamepad = u32(bytes, 8) & 2;
    for (unsigned i = 0; i < control_count; ++i) {
        value.keys[i] = u32(bytes, 12 + i * 4);
        value.buttons[i] = std::uint64_t(u32(bytes, 56 + i * 8)) | (std::uint64_t(u32(bytes, 60 + i * 8)) << 32);
    }
    value.validate();
    return value;
}
std::string HostSettings::key_name(unsigned key) {
    if ((key >= 'A' && key <= 'Z') || (key >= '0' && key <= '9'))
        return std::string(1, char(key));
    switch (key) {
        case 13:
            return "ENTER";
        case 27:
            return "ESC";
        case 32:
            return "SPACE";
        case 37:
            return "LEFT";
        case 38:
            return "UP";
        case 39:
            return "RIGHT";
        case 40:
            return "DOWN";
        case 160:
            return "L SHIFT";
        case 161:
            return "R SHIFT";
        case 162:
            return "L CTRL";
        case 163:
            return "R CTRL";
        default:
            return "KEY " + std::to_string(key);
    }
}
std::string HostSettings::button_name(std::uint64_t button) {
    if (!button)
        return "NONE";
    if (button == (pad_button::lb | pad_button::lt))
        return "LB/LT";
    if (button == (pad_button::rb | pad_button::rt))
        return "RB/RT";
    switch (button) {
        case pad_button::a:
            return "A / BTN1";
        case pad_button::b:
            return "B / BTN2";
        case pad_button::x:
            return "X / BTN3";
        case pad_button::y:
            return "Y / BTN4";
        case pad_button::lb:
            return "LB / BTN5";
        case pad_button::rb:
            return "RB / BTN6";
        case pad_button::back:
            return "BACK / BTN7";
        case pad_button::start:
            return "START / BTN8";
        case pad_button::lt:
            return "LT / BTN11";
        case pad_button::rt:
            return "RT / BTN12";
        case pad_button::left:
            return "DPAD LEFT";
        case pad_button::right:
            return "DPAD RIGHT";
        case pad_button::up:
            return "DPAD UP";
        case pad_button::down:
            return "DPAD DOWN";
        case pad_button::stick_left:
            return "STICK LEFT";
        case pad_button::stick_right:
            return "STICK RIGHT";
        case pad_button::stick_up:
            return "STICK UP";
        case pad_button::stick_down:
            return "STICK DOWN";
        default:
            return "BUTTON " + std::to_string(std::countr_zero(button) + 1);
    }
}
HostSettingsStore::HostSettingsStore(std::filesystem::path path, std::unique_ptr<SaveStorage> storage) :
    file_(std::move(path), "host.cfg", HostSettings::record_bytes, [](auto bytes) { HostSettings::parse(bytes); }, std::move(storage)) {}
std::optional<HostSettings> HostSettingsStore::load() {
    if (auto bytes = file_.load())
        return HostSettings::parse(*bytes);
    return {};
}
bool HostSettingsStore::save(const HostSettings& value) {
    return file_.save(value.serialize());
}
} // namespace web
