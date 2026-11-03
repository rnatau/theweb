#pragma once
#include "game_session.hpp"
#include "options.hpp"
#include "render/nudge_feedback.hpp"
#include "render/backbox_menu.hpp"
#include <memory>
#include <string_view>

namespace web {
class FrontendMenus;
class FrontendAssets;
struct HostSettings;
// A presentation strategy cannot access rules, physics or their RNG. trigger()
// returns true only when the front canvas must be submitted immediately before
// the original nudge sounds and following simulation tick.
class NudgePresentation {
public:
    virtual ~NudgePresentation() = default;
    virtual bool trigger(Bitmap& front, unsigned variant, std::uint32_t actions) = 0;
    virtual void advance(unsigned ticks) = 0;
    virtual void reset() = 0;
    virtual NudgeOffset offset(unsigned variant) const = 0;
};
struct Edition {
    std::string_view id, title, version, rules_version, physics_version;
    bool selectable_quality;
    bool selectable_nudge;
    bool pause_on_escape;
    void (*normalize_options)(GameOptions&);
    std::span<const BackboxMenu::Item> options_menu;
    std::span<const BackboxMenu::Item> (*main_menu)(bool session_available);
    GameOptions (*default_options)();
    std::unique_ptr<GameSession> (*create_game)(const std::filesystem::path&);
    std::unique_ptr<NudgePresentation> (*create_nudge)();
    std::unique_ptr<FrontendMenus> (*create_menu)(const DotMatrixRenderer&, const FrontendAssets&, GameOptions&, HostSettings&, bool) = nullptr;
    bool consolidated_menu = false;
    SoundRequestFilter normalize_sound = nullptr;
    std::string_view input_version = "frame-input-v1";
    bool timestamped_input = false;
    bool incremental_presentation = false;
};
// Exactly one edition translation unit is linked into each executable. There
// is no runtime switch and the faithful target cannot link Enhanced sources.
const Edition& selected_edition();
} // namespace web
