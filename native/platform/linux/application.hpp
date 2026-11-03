#pragma once
#include <filesystem>

namespace web {
struct Edition;
struct LinuxLaunch {
    bool fullscreen = true, explicit_display = false, mute = false, save = true;
    std::filesystem::path save_root;
    // Development host only; never accepted by a shipping entry point.
    bool smoke_test = false;
    std::filesystem::path smoke_output;
};
int run_linux(const LinuxLaunch&, const Edition&);
int linux_main(int argc, char** argv, bool development, const Edition&);
} // namespace web
