#pragma once
#include <windows.h>
#include <array>
#include <map>
#include <optional>

namespace web::windows {
constexpr bool keyboard_message(UINT message) {
    return message == WM_KEYDOWN || message == WM_KEYUP || message == WM_SYSKEYDOWN || message == WM_SYSKEYUP;
}
constexpr bool keyboard_pressed(UINT message) {
    return message == WM_KEYDOWN || message == WM_SYSKEYDOWN;
}
inline unsigned distinct_key(unsigned key, LPARAM flags) {
    if (key == VK_SHIFT)
        return MapVirtualKeyW((unsigned(flags) >> 16) & 255, MAPVK_VSC_TO_VK_EX);
    if (key == VK_CONTROL)
        return flags & (LPARAM(1) << 24) ? VK_RCONTROL : VK_LCONTROL;
    return key;
}
inline bool generic_modifier(unsigned key) {
    return key == VK_SHIFT || key == VK_CONTROL || key == VK_MENU;
}
inline void combine_modifiers(std::array<bool, 256>& held) {
    held[VK_SHIFT] = held[VK_LSHIFT] || held[VK_RSHIFT];
    held[VK_CONTROL] = held[VK_LCONTROL] || held[VK_RCONTROL];
    held[VK_MENU] = held[VK_LMENU] || held[VK_RMENU];
}
struct KeyboardTransition {
    unsigned key;
    bool down;
};
inline std::optional<KeyboardTransition> raw_key(const RAWKEYBOARD& packet) {
    if (packet.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE || !packet.VKey || packet.VKey >= 255)
        return {};
    const auto key = packet.VKey == VK_MENU ? unsigned((packet.Flags & RI_KEY_E0) ? VK_RMENU : VK_LMENU)
                                            : distinct_key(packet.VKey, (LPARAM(packet.MakeCode) << 16) | ((packet.Flags & RI_KEY_E0) ? LPARAM(1) << 24 : 0));
    if (!key || key >= 255)
        return {};
    return KeyboardTransition{key, (packet.Flags & RI_KEY_BREAK) == 0};
}
class RawKeyboardState {
public:
    bool update(HANDLE device, const RAWKEYBOARD& packet) {
        if (packet.MakeCode == KEYBOARD_OVERRUN_MAKE_CODE) {
            active_ = true;
            remove(device);
            return true;
        }
        const auto transition = raw_key(packet);
        if (!transition)
            return false;
        active_ = true;
        devices_[device][transition->key] = transition->down;
        compose();
        return true;
    }
    void remove(HANDLE device) {
        devices_.erase(device);
        compose();
    }
    void clear() {
        devices_.clear();
        held_.fill(false);
        fullscreen_enter_ = false;
    }
    bool active() const {
        return active_;
    }
    const std::array<bool, 256>& held() const {
        return held_;
    }

private:
    void compose() {
        held_.fill(false);
        for (const auto& [device, keys] : devices_)
            for (unsigned key = 0; key < keys.size(); ++key)
                held_[key] = held_[key] || keys[key];
        combine_modifiers(held_);
        if (!held_[VK_RETURN])
            fullscreen_enter_ = false;
        else if (held_[VK_MENU])
            fullscreen_enter_ = true;
        if (fullscreen_enter_)
            held_[VK_RETURN] = false;
    }
    bool active_ = false;
    bool fullscreen_enter_ = false;
    std::map<HANDLE, std::array<bool, 256>> devices_;
    std::array<bool, 256> held_{};
};
template <class IsDown, class Release> void release_missing_modifiers(const std::array<bool, 256>& held, const std::array<bool, 256>& suppressed, IsDown is_down, Release release) {
    // Windows can omit the first Shift release when both sides are held.
    // Reconcile only known modifiers, after processing queued input; never
    // synthesize a press or release the opposite side while it remains down.
    for (const unsigned key : {VK_LSHIFT, VK_RSHIFT, VK_LCONTROL, VK_RCONTROL})
        if ((held[key] || suppressed[key]) && !is_down(key))
            release(key);
}
} // namespace web::windows
