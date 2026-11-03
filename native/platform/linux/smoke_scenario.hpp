#pragma once
#include <SDL.h>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
namespace web::development {
class SmokeScenario {
    std::uint64_t frames_ = 0;
    unsigned stage_ = 0;

public:
    unsigned stage() const {
        return stage_;
    }
    template <class Host> void step(Host& host, unsigned tick) {

        // Drive the same SDL event path as a keyboard. The development host has
        // no mutation access to rules/physics. Assertions cover actual handoffs.
        const auto key = [&](SDL_Keycode value, bool down) {
            SDL_Event event{};
            event.type = down ? SDL_KEYDOWN : SDL_KEYUP;
            event.key.keysym.sym = value;
            event.key.state = down ? SDL_PRESSED : SDL_RELEASED;
            host.handle(event);
        };
        if (stage_ == 0) {
            key(SDLK_SPACE, true);
            key(SDLK_SPACE, false);
            key(SDLK_RETURN, true);
            key(SDLK_RETURN, false);
            stage_ = 1;
        } else if (stage_ == 1 && host.game_ && !host.menu_ && !host.game_->startup_ticks()) {
            key(SDLK_s, true);
            key(SDLK_s, false);
            stage_ = 2;
        } else if (stage_ == 2 && host.game_ && host.game_->player_count() && tick > 180) {
            key(SDLK_RETURN, true);
            stage_ = 3;
        } else if (stage_ == 3 && tick > 210) {
            key(SDLK_RETURN, false);
            if (!host.game_->launches())
                throw std::runtime_error("Smoke test: launcher did not fire");
            key(SDLK_g, true);
            key(SDLK_g, false);
            stage_ = 31;
        } else if (stage_ == 31 && tick > 230) {
            if (!host.nudge_presentations_)
                throw std::runtime_error("Smoke test: nudge did not reach the presentation boundary");
            frames_ = host.game_->rule_state().frame_phase();
            key(SDLK_ESCAPE, true);
            key(SDLK_ESCAPE, false);
            stage_ = 4;
        } else if (stage_ == 4 && tick > 240) {
            if (!host.menu_)
                throw std::runtime_error("Smoke test: Escape did not open menu");
            if (host.edition_.pause_on_escape && (!host.game_ || host.game_->rule_state().frame_phase() != frames_))
                throw std::runtime_error("Smoke test: retained session advanced while paused");
            if (!host.edition_.pause_on_escape && host.game_)
                throw std::runtime_error("Smoke test: Classic did not release session");
            key(SDLK_RETURN, true);
            key(SDLK_RETURN, false);
            stage_ = 5;
        } else if (stage_ == 5 && !host.menu_ && tick > 310) {
            if (!host.game_ || (host.edition_.pause_on_escape && host.game_->rule_state().frame_phase() <= frames_))
                throw std::runtime_error("Smoke test: PLAY/RESUME did not progress");
            frames_ = host.game_->rule_state().frame_phase();
            SDL_Event lost{};
            lost.type = SDL_WINDOWEVENT;
            lost.window.event = SDL_WINDOWEVENT_FOCUS_LOST;
            host.handle(lost);
            stage_ = 6;
        } else if (stage_ == 6 && tick > 330) {
            if (host.game_->rule_state().frame_phase() != frames_)
                throw std::runtime_error("Smoke test: focus loss advanced simulation");
            SDL_Event gained{};
            gained.type = SDL_WINDOWEVENT;
            gained.window.event = SDL_WINDOWEVENT_FOCUS_GAINED;
            host.handle(gained);
            stage_ = 7;
        } else if (stage_ == 7 && tick > 360 && !host.game_->startup_ticks() && host.game_->rule_state().frame_phase() > 0) {
            host.present();
            if (!host.launch_.smoke_output.empty()) {
                std::filesystem::create_directories(host.launch_.smoke_output);
                auto* surface = SDL_CreateRGBSurfaceWithFormatFrom(host.output_.data(), host.texture_width_, host.texture_height_, 32, host.texture_width_ * 4, SDL_PIXELFORMAT_ARGB8888);
                if (!surface || SDL_SaveBMP(surface, (host.launch_.smoke_output / (std::string(host.edition_.id) + ".bmp")).c_str()) < 0)
                    throw std::runtime_error(SDL_GetError());
                SDL_FreeSurface(surface);
            }
            const auto color_frame = host.screen_.pixels;
            key(SDLK_d, true);
            key(SDLK_d, false);
            if (host.screen_.pixels != color_frame)
                throw std::runtime_error("Smoke test: D changed the shipping table presentation");
            const auto old_mode = host.launch_.fullscreen;
            SDL_Event toggle{};
            toggle.type = SDL_KEYDOWN;
            toggle.key.keysym.sym = SDLK_RETURN;
            toggle.key.keysym.mod = KMOD_ALT;
            host.handle(toggle);
            if (host.launch_.fullscreen == old_mode)
                throw std::runtime_error("Smoke test: Alt+Enter did not toggle fullscreen");
            host.handle(toggle);
            SDL_SetWindowSize(host.window_, 800, 600);
            key(SDLK_ESCAPE, true);
            key(SDLK_ESCAPE, false);
            stage_ = 8;
        } else if (stage_ == 8 && host.menu_ && !host.pending_menu_ && host.focused_ && !host.minimized_) {
            key(SDLK_DOWN, true);
            key(SDLK_DOWN, false);
            stage_ = 9;
        } else if (stage_ == 9 && host.menu_->selected_id() == 103) {
            key(SDLK_RETURN, true);
            key(SDLK_RETURN, false);
            stage_ = 10;
        } else if (stage_ == 10 && host.menu_->page() && !host.pending_menu_) {
            key(SDLK_ESCAPE, true);
            key(SDLK_ESCAPE, false);
            stage_ = 11;
        } else if (stage_ == 11 && !host.menu_->page()) {
            key(SDLK_DOWN, true);
            key(SDLK_DOWN, false);
            stage_ = 12;
        } else if (stage_ == 12 && host.menu_->selected_id() == 103) {
            key(SDLK_DOWN, true);
            key(SDLK_DOWN, false);
            stage_ = 13;
        } else if (stage_ == 13 && host.menu_->selected_id() == 102) {
            key(SDLK_RETURN, true);
            key(SDLK_RETURN, false);
            stage_ = 14;
        } else if (stage_ == 14 && host.slides_) {
            key(SDLK_RIGHT, true);
            key(SDLK_RIGHT, false);
            stage_ = 15;
        } else if (stage_ == 15 && host.slides_->image() == 1) {
            key(SDLK_ESCAPE, true);
            key(SDLK_ESCAPE, false);
            stage_ = 16;
        } else if (stage_ == 16 && host.menu_ && !host.pending_menu_) {
            key(SDLK_RETURN, true);
            key(SDLK_RETURN, false);
            stage_ = 17;
        } else if (stage_ == 17 && host.game_ && !host.menu_ && !host.game_->startup_ticks() && host.game_->rule_state().frame_phase() > 0) {
            if (!host.edition_.consolidated_menu) {
                key(SDLK_F2, true);
                key(SDLK_F2, false);
                frames_ = host.options_.camera_selection();
                stage_ = 18;
            } else
                stage_ = 20;
        } else if (stage_ == 18 && host.classic_options_) {
            key(SDLK_RETURN, true);
            key(SDLK_RETURN, false);
            stage_ = 19;
        } else if (stage_ == 19 && host.options_.camera_selection() != frames_) {
            key(SDLK_ESCAPE, true);
            key(SDLK_ESCAPE, false);
            if (!host.table_ || host.table_->variant() != host.options_.display_variant())
                throw std::runtime_error("Smoke test: F2 camera did not apply");
            stage_ = 20;
        } else if (stage_ == 20) {
            if (!host.edition_.consolidated_menu) {
                frames_ = host.game_->rule_state().frame_phase();
                key(SDLK_F4, true);
                key(SDLK_F4, false);
                stage_ = 21;
            } else
                stage_ = 25;
        } else if (stage_ == 21 && host.menu_) {
            if (!host.game_ || host.game_->rule_state().frame_phase() != frames_)
                throw std::runtime_error("Smoke test: F4 did not retain the paused Classic session");
            key(SDLK_ESCAPE, true);
            key(SDLK_ESCAPE, false);
            if (!host.game_ || host.menu_ || host.game_->rule_state().frame_phase() != frames_)
                throw std::runtime_error("Smoke test: F4 Escape did not resume the retained Classic session");
            stage_ = 25;
        } else if (stage_ == 25) {
            std::cout << "PASS Linux " << host.edition_.id
                      << ": SDL display/input, startup/start/launch/nudge, menu lifetime, focus suspension, resume, fullscreen/resize, options, slideshow, checkpoint; updates="
                      << host.game_->rule_state().frame_phase() << '\n';
            host.quit_ = true;
        }
        // Keep the harness ticking during synthetic focus suspension while the
        // actual game remains suspended by the ordinary event-loop condition.
    }
};
} // namespace web::development
