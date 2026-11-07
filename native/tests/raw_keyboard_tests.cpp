#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../platform/win32/keyboard_input.hpp"
#include "../host_settings.hpp"
#include "../input_timeline.hpp"
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
RAWKEYBOARD packet(unsigned key, unsigned scan, bool down, bool extended = false) {
    RAWKEYBOARD value{};
    value.VKey = USHORT(key);
    value.MakeCode = USHORT(scan);
    value.Flags = USHORT((down ? RI_KEY_MAKE : RI_KEY_BREAK) | (extended ? RI_KEY_E0 : 0));
    // Decoding must follow raw flags, regardless of legacy message metadata.
    value.Message = down ? WM_SYSKEYUP : WM_KEYDOWN;
    return value;
}
void combinations() {
    using namespace web;
    using namespace web::windows;
    for (unsigned pair = 0; pair < 3; ++pair)
        for (unsigned release_first = 0; release_first < 2; ++release_first) {
            RawKeyboardState keyboard;
            HostSettings bindings;
            const unsigned left = pair == 0 ? 'F' : pair == 1 ? VK_LCONTROL : VK_LSHIFT;
            const unsigned right = pair == 0 ? 'J' : pair == 1 ? VK_RCONTROL : VK_RSHIFT;
            bindings.bind_key(0, left);
            bindings.bind_key(1, right);
            const auto key_packet = [&](unsigned side, bool down) {
                const auto key = side ? right : left;
                return packet(pair == 0 ? key : pair == 1 ? VK_CONTROL : VK_SHIFT, pair == 0 ? (side ? 0x24 : 0x21) : pair == 1 ? 0x1d : side ? 0x36 : 0x2a, down, pair == 1 && side);
            };
            InputTimeline timeline;
            timeline.reset(0);
            const auto update = [&](unsigned time, RAWKEYBOARD value) {
                check(keyboard.update(nullptr, value), "Valid raw packet was discarded");
                timeline.record({std::uint64_t(time) * 1000000, InputSource::keyboard, bindings.keyboard_actions(keyboard.held())});
            };
            update(1, key_packet(0, true));
            update(2, key_packet(1, true));
            check(bindings.keyboard_actions(keyboard.held()) == input_action::both_flippers, "Raw flipper pair was not independent");
            update(3, packet('G', 0x22, true));
            update(4, key_packet(0, true)); // Auto-repeat cannot change ownership.
            update(5, key_packet(release_first, false));
            const auto remaining = release_first ? input_action::left_flipper : input_action::right_flipper;
            check(bindings.keyboard_actions(keyboard.held()) == (remaining | input_action::nudge_negative), "Raw release cleared the other held key or stayed stuck");
            update(9, key_packet(1 - release_first, false));
            update(12, packet('G', 0x22, false));
            check(bindings.keyboard_actions(keyboard.held()) == 0, "Raw chord retained input after all releases");
            const auto frame = timeline.frame(0, 17000000, 17);
            for (unsigned step = 0; step < 17; ++step) {
                const auto flippers = step < 1 ? 0 : step < 2 ? input_action::left_flipper : step < 5 ? input_action::both_flippers : step < 9 ? remaining : 0;
                const auto nudge = step >= 3 && step < 12 ? input_action::nudge_negative : 0;
                check(frame.microsteps[step].actions == (flippers | nudge), "Raw chord reached the wrong physics microstep");
            }
            check(keyboard.active(), "Raw input ownership was not retained");
            keyboard.clear();
            check(keyboard.active() && bindings.keyboard_actions(keyboard.held()) == 0, "Focus loss retained keys or re-enabled duplicate legacy input");
        }
}
void devices() {
    using namespace web::windows;
    RawKeyboardState keyboard;
    const auto a = reinterpret_cast<HANDLE>(std::uintptr_t(1)), b = reinterpret_cast<HANDLE>(std::uintptr_t(2));
    keyboard.update(a, packet('F', 0x21, true));
    keyboard.update(b, packet('F', 0x21, true));
    keyboard.update(a, packet('F', 0x21, false));
    check(keyboard.held()['F'], "One keyboard released a key still held on another keyboard");
    keyboard.remove(b);
    check(!keyboard.held()['F'], "Keyboard removal retained its held key");
    keyboard.update(a, packet(VK_CONTROL, 0x1d, true));
    keyboard.update(b, packet(VK_CONTROL, 0x1d, true, true));
    check(keyboard.held()[VK_CONTROL], "Generic Ctrl binding was not retained");
    web::GameOptions classic;
    classic.key(0, VK_CONTROL);
    check((classic.actions(keyboard.held()) & web::input_action::left_flipper) != 0, "Original generic Ctrl binding did not accept raw side-specific keys");
    keyboard.update(a, packet(0, KEYBOARD_OVERRUN_MAKE_CODE, true));
    check(!keyboard.held()[VK_LCONTROL] && keyboard.held()[VK_RCONTROL] && keyboard.held()[VK_CONTROL], "Overrun failed to release only the affected keyboard");
    keyboard.update(b, packet(VK_CONTROL, 0x1d, false, true));
    check(!keyboard.held()[VK_CONTROL], "Generic Ctrl remained held after the final release");
    check(!raw_key(packet(255, 0x21, true)) && !raw_key(packet(0, 0, true)), "Invalid raw virtual key was accepted");
    check(raw_key(packet(VK_MENU, 0x38, true, true))->key == VK_RMENU, "Right Alt was conflated with Ctrl");
    keyboard.clear();
    keyboard.update(a, packet(VK_MENU, 0x38, true));
    keyboard.update(a, packet(VK_RETURN, 0x1c, true));
    check(!keyboard.held()[VK_RETURN], "Alt+Enter leaked a launch command");
    keyboard.update(a, packet(VK_MENU, 0x38, false));
    check(!keyboard.held()[VK_RETURN], "Releasing Alt launched a still-held fullscreen shortcut");
    keyboard.update(a, packet(VK_RETURN, 0x1c, false));
    keyboard.update(a, packet(VK_RETURN, 0x1c, true));
    check(keyboard.held()[VK_RETURN], "Fullscreen shortcut suppressed a fresh launch press");
}
void registration() {
    // This is a private message-only window in the test process. No desktop
    // input is injected and no other process's registration is changed.
    const auto window = CreateWindowExW(0, L"STATIC", L"Raw keyboard fixture", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, GetModuleHandleW(nullptr), nullptr);
    check(window != nullptr, "Raw fixture window creation failed");
    RAWINPUTDEVICE device{0x01, 0x06, RIDEV_DEVNOTIFY, window};
    const bool registered = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    UINT count = 1;
    RAWINPUTDEVICE actual{};
    const auto copied = GetRegisteredRawInputDevices(&actual, &count, sizeof(actual));
    device.dwFlags = RIDEV_REMOVE;
    device.hwndTarget = nullptr;
    const bool removed = RegisterRawInputDevices(&device, 1, sizeof(device)) != FALSE;
    DestroyWindow(window);
    check(registered && copied == 1 && count == 1 && actual.usUsagePage == 1 && actual.usUsage == 6 && actual.hwndTarget == window, "Foreground raw keyboard registration failed");
    check((actual.dwFlags & (RIDEV_NOLEGACY | RIDEV_INPUTSINK | RIDEV_EXINPUTSINK)) == 0 && removed, "Raw registration suppressed menus, enabled background capture or failed removal");
}
} // namespace
int main() {
    try {
        combinations();
        devices();
        registration();
        std::cout << "PASS: " << checks << " raw keyboard decoding/chords/devices/timing/registration checks\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
