#include "../platform/linux/audio.hpp"
#include <SDL.h>
#include <iostream>

int main() {
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    try {
        // No media is needed: exercise repeated worker startup and shutdown,
        // including shutdown while the device is paused or actively playing.
        for (unsigned i = 0; i < 32; ++i) {
            web::LinuxMusic music(std::filesystem::temp_directory_path());
            music.pause(false);
            music.update({}, 128);
            SDL_Delay(6);
            if (i % 2)
                music.pause(true);
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        SDL_Quit();
        return 1;
    }
    SDL_Quit();
    std::cout << "PASS music worker startup/shutdown\n";
}
