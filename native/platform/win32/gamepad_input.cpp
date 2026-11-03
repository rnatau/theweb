#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define DIRECTINPUT_VERSION 0x0800
#include <windows.h>
#include <xinput.h>
#include <dinput.h>
#include <utility>
#include "gamepad_input.hpp"

namespace web {
class GamepadInput::Impl {
public:
    using GetState = DWORD(WINAPI*)(DWORD, XINPUT_STATE*);
    HMODULE library = nullptr;
    GetState get_state = nullptr;
    IDirectInput8W* direct = nullptr;
    IDirectInputDevice8W* device = nullptr;
    bool scan_pending = true;
    std::uint64_t previous = 0;
    bool axes_normalized = false;
    bool has_pov = false;
    Impl() {
        for (auto name : {L"xinput1_4.dll", L"xinput9_1_0.dll"}) {
            library = LoadLibraryExW(name, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (library) {
                get_state = reinterpret_cast<GetState>(reinterpret_cast<void*>(GetProcAddress(library, "XInputGetState")));
                if (get_state)
                    break;
                FreeLibrary(library);
                library = nullptr;
            }
        }
    }
    ~Impl() {
        release_device();
        if (direct)
            direct->Release();
        if (library)
            FreeLibrary(library);
    }
    void release_device() {
        if (device) {
            device->Unacquire();
            device->Release();
            device = nullptr;
        }
        axes_normalized = false;
        has_pov = false;
    }
    static BOOL CALLBACK enumerate(const DIDEVICEINSTANCEW* instance, void* context) {
        auto& self = *static_cast<Impl*>(context);
        if (SUCCEEDED(self.direct->CreateDevice(instance->guidInstance, &self.device, nullptr)))
            return DIENUM_STOP;
        return DIENUM_CONTINUE;
    }
    GamepadReading poll(HWND window) {
        if (get_state)
            for (DWORD index = 0; index < XUSER_MAX_COUNT; ++index) {
                XINPUT_STATE state{};
                if (get_state(index, &state) != ERROR_SUCCESS)
                    continue;
                const auto& pad = state.Gamepad;
                GamepadSample sample;
                constexpr std::array<WORD, 10> bits = {
                    XINPUT_GAMEPAD_A,
                    XINPUT_GAMEPAD_B,
                    XINPUT_GAMEPAD_X,
                    XINPUT_GAMEPAD_Y,
                    XINPUT_GAMEPAD_LEFT_SHOULDER,
                    XINPUT_GAMEPAD_RIGHT_SHOULDER,
                    XINPUT_GAMEPAD_BACK,
                    XINPUT_GAMEPAD_START,
                    XINPUT_GAMEPAD_LEFT_THUMB,
                    XINPUT_GAMEPAD_RIGHT_THUMB};
                for (unsigned i = 0; i < bits.size(); ++i)
                    if (pad.wButtons & bits[i])
                        sample.buttons |= 1ull << i;
                if (pad.wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
                    sample.buttons |= pad_button::left;
                if (pad.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
                    sample.buttons |= pad_button::right;
                if (pad.wButtons & XINPUT_GAMEPAD_DPAD_UP)
                    sample.buttons |= pad_button::up;
                if (pad.wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
                    sample.buttons |= pad_button::down;
                sample.x = pad.sThumbLX;
                sample.y = pad.sThumbLY;
                sample.left_trigger = pad.bLeftTrigger;
                sample.right_trigger = pad.bRightTrigger;
                previous = gamepad_buttons(sample, previous);
                return {previous, true};
            }
        if (!device && std::exchange(scan_pending, false)) {
            if (!direct && FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, reinterpret_cast<void**>(&direct), nullptr)))
                return {};
            direct->EnumDevices(DI8DEVCLASS_GAMECTRL, enumerate, this, DIEDFL_ATTACHEDONLY);
            if (device) {
                DIPROPRANGE range{};
                range.diph.dwSize = sizeof(range);
                range.diph.dwHeaderSize = sizeof(range.diph);
                range.diph.dwHow = DIPH_DEVICE;
                range.lMin = -32768;
                range.lMax = 32767;
                if (FAILED(device->SetDataFormat(&c_dfDIJoystick2)) || FAILED(device->SetCooperativeLevel(window, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE)))
                    release_device();
                if (device) {
                    axes_normalized = SUCCEEDED(device->SetProperty(DIPROP_RANGE, &range.diph));
                    DIDEVCAPS capabilities{};
                    capabilities.dwSize = sizeof(capabilities);
                    has_pov = SUCCEEDED(device->GetCapabilities(&capabilities)) && capabilities.dwPOVs != 0;
                    device->Acquire();
                }
            }
        }
        if (device) {
            if (FAILED(device->Poll())) {
                device->Acquire();
                device->Poll();
            }
            DIJOYSTATE2 state{};
            if (SUCCEEDED(device->GetDeviceState(sizeof(state), &state))) {
                GamepadSample sample;
                for (unsigned i = 0; i < 32; ++i)
                    if (state.rgbButtons[i] & 0x80)
                        sample.buttons |= 1ull << i;
                if (axes_normalized) {
                    sample.x = state.lX;
                    sample.y = -state.lY;
                }
                const auto pov = state.rgdwPOV[0];
                if (has_pov && pov != 0xffffffff && LOWORD(pov) != 0xffff) {
                    if (pov >= 22500 && pov <= 31500)
                        sample.buttons |= pad_button::left;
                    if (pov >= 4500 && pov <= 13500)
                        sample.buttons |= pad_button::right;
                    if (pov <= 4500 || pov >= 31500)
                        sample.buttons |= pad_button::up;
                    if (pov >= 13500 && pov <= 22500)
                        sample.buttons |= pad_button::down;
                }
                previous = gamepad_buttons(sample, previous);
                return {previous, true};
            }
            // Keep the device through transient acquisition/focus loss. The
            // next poll retries Acquire; a device-tree notification replaces
            // removed devices without periodically enumerating an empty bus.
        }
        previous = 0;
        return {};
    }
};
GamepadInput::GamepadInput() :
    impl_(std::make_unique<Impl>()) {}
GamepadInput::~GamepadInput() = default;
GamepadReading GamepadInput::poll(void* window) {
    return impl_->poll(static_cast<HWND>(window));
}
void GamepadInput::devices_changed() {
    impl_->release_device();
    impl_->scan_pending = true;
}
} // namespace web
