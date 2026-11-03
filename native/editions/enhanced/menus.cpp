#include "menus.hpp"
#include <algorithm>

namespace web::enhanced {
Menus::Menus(const DotMatrixRenderer& renderer, const FrontendAssets& assets, GameOptions& options, HostSettings& settings, bool session_available) :
    FrontendMenus(renderer, assets, options),
    renderer_(renderer),
    options_(options),
    settings_(settings),
    session_available_(session_available),
    camera_(assets) {
    if (options.music_volume())
        last_music_volume_ = options.music_volume();
    rebuild();
}
void Menus::open(Page page, int selected) {
    page_ = page;
    capture_ = -1;
    feedback_.clear();
    if (page == camera) {
        camera_.select(400 + options_.camera_selection());
        camera_.remember();
    } else
        rebuild(selected);
}
void Menus::rebuild(int selected) {
    std::vector<int> ids;
    labels_.clear();
    const auto add = [&](int id, std::string label) {
        ids.push_back(id);
        labels_.push_back(std::move(label));
    };
    switch (page_) {
        case main:
            add(101, session_available_ ? "RESUME" : "PLAY");
            add(103, "OPTIONS");
            add(102, "SLIDESHOW");
            add(104, "QUIT");
            break;
        case options:
            add(207, "CONTROLS");
            add(204, "TABLE VIEW");
            add(208, "SOUND");
            add(209, "FULLSCREEN");
            add(202, "SCREEN FLIP");
            add(206, "MAIN MENU");
            break;
        case controls:
            add(701, "KEYBOARD");
            add(702, "GAMEPAD");
            add(703, settings_.gamepad ? "GAMEPAD ON" : "GAMEPAD OFF");
            add(799, "BACK");
            break;
        case keyboard:
        case gamepad:
            for (unsigned i = 0; i < HostSettings::control_count; ++i)
                add(800 + int(i), std::string(HostSettings::labels[i]));
            add(850, "DEFAULTS");
            add(899, "BACK");
            break;
        case sound:
            add(901, options_.sound_enabled() ? "EFFECTS ON" : "EFFECTS OFF");
            add(902, "EFFECTS VOLUME");
            add(903, options_.music_enabled() ? "MUSIC ON" : "MUSIC OFF");
            add(904, "MUSIC VOLUME");
            add(205, "MUSIC RESTART");
            add(999, "BACK");
            break;
        case effects_volume:
        case music_volume:
            for (unsigned percent : {0u, 25u, 50u, 75u, 100u})
                add(1000 + int(percent), "VOLUME " + std::to_string(percent) + "%");
            add(1099, "BACK");
            break;
        case fullscreen:
            add(1101, "FULLSCREEN");
            add(1102, "WINDOWED");
            break;
        case flip:
            add(351, "FLIP ENABLED");
            add(352, "FLIP DISABLED");
            break;
        case policy:
            add(501, "INSTANT RESTART");
            add(502, "QUIET BALL");
            add(503, "NO TABLE BALLS");
            break;
        case camera:
            break;
    }
    std::vector<BackboxMenu::Item> items;
    for (unsigned i = 0; i < ids.size(); ++i)
        items.push_back({ids[i], labels_[i]});
    list_ = std::make_unique<BackboxMenu>(renderer_, items);
    if (selected && std::find(ids.begin(), ids.end(), selected) != ids.end())
        list_->select(selected);
}
bool Menus::input(std::uint32_t edges, SoundSink* sounds) {
    if (capture_ >= 0)
        return false;
    return page_ == camera ? camera_.input(edges, sounds) : list_->input(edges, sounds);
}
bool Menus::activation_complete() const {
    return activation_remaining() < 1;
}
FrontendMenus::Action Menus::accept() {
    const int id = selected_id();
    if (page_ == main) {
        if (id == 101)
            return Action::play;
        if (id == 102)
            return Action::slideshow;
        if (id == 104)
            return Action::quit;
        open(options);
    } else if (page_ == options) {
        switch (id) {
            case 207:
                open(controls);
                break;
            case 204:
                open(camera);
                break;
            case 208:
                open(sound);
                break;
            case 209:
                open(fullscreen, settings_.fullscreen ? 1101 : 1102);
                break;
            case 202:
                open(flip, options_.screen_flip() ? 351 : 352);
                break;
            case 206:
                open(main);
                break;
        }
    } else if (page_ == controls) {
        if (id == 701)
            open(keyboard);
        else if (id == 702)
            open(gamepad);
        else if (id == 703) {
            settings_.gamepad = !settings_.gamepad;
            rebuild(703);
        } else
            open(options, 207);
    } else if (page_ == keyboard || page_ == gamepad) {
        if (id == 899)
            open(controls, page_ == keyboard ? 701 : 702);
        else if (id == 850) {
            if (page_ == keyboard)
                settings_.keys = HostSettings{}.keys;
            else
                settings_.buttons = HostSettings{}.buttons;
            rebuild(850);
        } else {
            capture_ = id - 800;
            feedback_.clear();
        }
    } else if (page_ == sound) {
        if (id == 901) {
            options_.sound_enabled(!options_.sound_enabled());
            rebuild(901);
        } else if (id == 902 || id == 904) {
            const unsigned maximum = id == 902 ? 127 : 255;
            const unsigned value = id == 902 ? options_.volume() : options_.music_volume();
            open(id == 902 ? effects_volume : music_volume, 1000 + int(((value * 4 + maximum / 2) / maximum) * 25));
        } else if (id == 903) {
            if (options_.music_volume()) {
                last_music_volume_ = options_.music_volume();
                options_.music_volume(0);
            } else
                options_.music_volume(last_music_volume_);
            rebuild(903);
        } else if (id == 205)
            open(policy, 501 + int(options_.music_restart_policy()));
        else
            open(options, 208);
    } else if (page_ == effects_volume || page_ == music_volume) {
        const auto previous = page_;
        if (id != 1099) {
            if (page_ == effects_volume)
                options_.volume(unsigned(id - 1000) * 127 / 100);
            else {
                options_.music_volume(unsigned(id - 1000) * 255 / 100);
                if (options_.music_volume())
                    last_music_volume_ = options_.music_volume();
            }
        }
        open(sound, previous == effects_volume ? 902 : 904);
    } else if (page_ == fullscreen) {
        settings_.fullscreen = id == 1101;
        open(options, 209);
        return Action::fullscreen;
    } else if (page_ == camera) {
        options_.camera_selection(unsigned(id - 400));
        open(options, 204);
    } else if (page_ == flip) {
        options_.screen_flip(id == 351);
        open(options, 202);
    } else if (page_ == policy) {
        options_.music_restart_policy(unsigned(id - 501));
        open(sound, 205);
    }
    return Action::none;
}
FrontendMenus::Action Menus::escape() {
    if (capture_ >= 0) {
        capture_ = -1;
        feedback_.clear();
        return Action::none;
    }
    switch (page_) {
        case main:
            return Action::quit;
        case options:
            open(main);
            break;
        case controls:
            open(options, 207);
            break;
        case keyboard:
            open(controls, 701);
            break;
        case gamepad:
            open(controls, 702);
            break;
        case sound:
            open(options, 208);
            break;
        case effects_volume:
            open(sound, 902);
            break;
        case music_volume:
            open(sound, 904);
            break;
        case policy:
            open(sound, 205);
            break;
        case fullscreen:
            open(options, 209);
            break;
        case flip:
            open(options, 202);
            break;
        case camera:
            camera_.restore();
            open(options, 204);
            break;
    }
    return Action::none;
}
bool Menus::capture_key(unsigned key) {
    if (capture_ < 0)
        return false;
    if (key == key_code::escape) {
        escape();
        return true;
    }
    if (page_ == gamepad)
        return true;
    try {
        settings_.bind_key(unsigned(capture_), key);
        capture_ = -1;
        feedback_.clear();
    } catch (const std::exception& error) {
        feedback_ = error.what();
    }
    return true;
}
bool Menus::capture_gamepad(std::uint64_t buttons) {
    if (capture_ < 0)
        return false;
    if (buttons == (pad_button::back | pad_button::start)) {
        escape();
        return true;
    }
    if (page_ == gamepad && buttons) {
        try {
            settings_.bind_button(unsigned(capture_), buttons);
            capture_ = -1;
            feedback_.clear();
        } catch (const std::exception& error) {
            feedback_ = error.what();
        }
    }
    return true;
}
DotMatrixFrame Menus::step(const DotMatrixFrame& backdrop, std::int32_t elapsed) {
    if (capture_ >= 0) {
        DotMatrixFrame frame{};
        renderer_.text(frame, HostSettings::labels[unsigned(capture_)], 1, 1, 1);
        const auto binding = page_ == keyboard ? HostSettings::key_name(settings_.keys[unsigned(capture_)]) : HostSettings::button_name(settings_.buttons[unsigned(capture_)]);
        renderer_.text(frame, feedback_.empty() ? binding : feedback_, 1, 1, 11);
        renderer_.text(frame, page_ == keyboard ? "KEY / ESC CANCEL" : "BUTTON / ESC BACK", 1, 1, 21);
        return frame;
    }
    return page_ == camera ? camera_.step(backdrop, elapsed) : list_->step(backdrop, elapsed);
}
std::int32_t Menus::selected_id() const {
    return page_ == camera ? camera_.selection().selected_id() : list_->selection().selected_id();
}
unsigned Menus::first_visible() const {
    return page_ == camera ? camera_.first_visible() : list_->first_visible();
}
std::uint32_t Menus::counter() const {
    return page_ == camera ? camera_.counter() : list_->counter();
}
std::int32_t Menus::activation_remaining() const {
    return page_ == camera ? camera_.activation_remaining() : list_->activation_remaining();
}
} // namespace web::enhanced
