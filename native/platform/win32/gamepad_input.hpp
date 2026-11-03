#pragma once
#include "../../host_settings.hpp"
#include <memory>

namespace web {
struct GamepadReading {
    std::uint64_t buttons = 0;
    bool connected = false;
};
// Built-in Windows APIs only: prefer XInput, then generic DirectInput devices.
class GamepadInput {
public:
    GamepadInput();
    ~GamepadInput();
    GamepadReading poll(void* window);
    // Windows broadcasts device-tree changes to the owning top-level window.
    // Discovery is requested at startup and on hotplug, outside steady polling.
    void devices_changed();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace web
