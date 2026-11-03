#pragma once
#include "options.hpp"
#include "save_file.hpp"

namespace web {
namespace host_action {
inline constexpr std::uint32_t pause = 0x10000, view = 0x20000, menu = 0x40000;
inline constexpr std::uint32_t commands = pause | view | menu;
} // namespace host_action
namespace pad_button {
inline constexpr std::uint64_t a = 1, b = 2, x = 4, y = 8, lb = 16, rb = 32, back = 64, start = 128;
inline constexpr std::uint64_t lt = 1ull << 10, rt = 1ull << 11;
inline constexpr std::uint64_t left = 1ull << 32, right = 1ull << 33, up = 1ull << 34, down = 1ull << 35;
inline constexpr std::uint64_t stick_left = 1ull << 36, stick_right = 1ull << 37, stick_up = 1ull << 38, stick_down = 1ull << 39;
inline constexpr std::uint64_t all = (1ull << 40) - 1;
} // namespace pad_button
struct GamepadSample {
    std::uint64_t buttons = 0;
    int x = 0, y = 0, left_trigger = 0, right_trigger = 0;
};
// Half travel engages; a wider release threshold prevents noisy axis edges.
std::uint64_t gamepad_buttons(GamepadSample, std::uint64_t previous);
// Enhanced host preferences do not change the original wgame.cfg format.
// Both backends normalize controller buttons/axes to this portable bit mask.
struct HostSettings {
    static constexpr unsigned control_count = 11;
    static constexpr std::size_t record_bytes = 144;
    static constexpr std::array<std::string_view, control_count> labels =
        {"LEFT FLIPPER", "RIGHT FLIPPER", "NUDGE LEFT", "NUDGE RIGHT", "NUDGE UP", "LAUNCH", "START", "CONTINUE", "PAUSE", "TABLE VIEW", "MENU"};
    std::array<unsigned, control_count> keys = {'F', 'J', 'G', 'H', key_code::space, key_code::enter, 'S', 'C', 'P', 'V', key_code::escape};
    std::array<std::uint64_t, control_count> buttons = {
        pad_button::lb | pad_button::lt,
        pad_button::rb | pad_button::rt,
        pad_button::stick_left,
        pad_button::stick_right,
        pad_button::stick_up,
        pad_button::a,
        pad_button::start,
        pad_button::x,
        pad_button::back,
        pad_button::y,
        pad_button::b};
    bool fullscreen = true, gamepad = true;
    void bind_key(unsigned control, unsigned key);
    void bind_button(unsigned control, std::uint64_t button);
    void validate() const;
    std::uint32_t keyboard_actions(const std::array<bool, 256>&) const;
    std::uint32_t gamepad_actions(std::uint64_t) const;
    Bytes serialize() const;
    static HostSettings parse(std::span<const std::uint8_t>);
    static std::string key_name(unsigned);
    static std::string button_name(std::uint64_t);
};
class HostSettingsStore {
public:
    HostSettingsStore(std::filesystem::path, std::unique_ptr<SaveStorage>);
    std::optional<HostSettings> load();
    bool save(const HostSettings&);
    const std::string& error() const {
        return file_.error();
    }

private:
    SaveFile file_;
};
} // namespace web
