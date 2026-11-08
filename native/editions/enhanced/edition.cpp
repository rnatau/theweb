#include "../../edition.hpp"
#include "../../gameplay.hpp"
#include "nudge_animation.hpp"
#include "start_rules.hpp"
#include "menus.hpp"
#include "mode_policy.hpp"
#include "corrections.hpp"

namespace web {
namespace {
constexpr std::string_view version = "1.60b";
class DirectionalNudge final : public NudgePresentation {
public:
    bool trigger(Bitmap&, unsigned, std::uint32_t actions) override {
        animation_.trigger(actions);
        return false;
    }
    void advance(unsigned ticks) override {
        animation_.advance(ticks);
    }
    void reset() override {
        animation_ = {};
    }
    NudgeOffset offset(unsigned variant) const override {
        return animation_.offset(variant);
    }

private:
    NudgeAnimation animation_;
};
void normalize_options(GameOptions& value) {
    auto configuration = value.configuration();
    configuration[configuration_field::screen_mode] = 6;
    configuration[configuration_field::graphic_detail] = 3;
    configuration[configuration_field::sound_rate] = value.sound_enabled() ? 44100 : 0;
    configuration[configuration_field::sound_stereo] = 1;
    configuration[configuration_field::sound_16bit] = 1;
    configuration[configuration_field::sound_channels] = 3;
    configuration[configuration_field::visual_nudge] = 1;
    value = GameOptions(configuration);
}
GameOptions options() {
    GameOptions value;
    value.visual_nudge(true);
    normalize_options(value);
    return value;
}
// Reuse the recovered simulation with Enhanced start and collection policies.
std::unique_ptr<GameSession> game(const std::filesystem::path& assets) {
    auto result = std::make_unique<PinballGame>(assets, PinballGame::InputTiming::timestamped, enhanced::start, enhanced::HoldStart{}, enhanced::collection_allowed, enhanced::corrections());
    result->startup_version(version);
    return result;
}
std::unique_ptr<NudgePresentation> nudge() {
    return std::make_unique<DirectionalNudge>();
}
std::unique_ptr<FrontendMenus> menu(const DotMatrixRenderer& renderer, const FrontendAssets& assets, GameOptions& options, HostSettings& settings, bool session_available) {
    return std::make_unique<enhanced::Menus>(renderer, assets, options, settings, session_available);
}
std::span<const BackboxMenu::Item> main_menu(bool session_available) {
    static constexpr std::array<BackboxMenu::Item, 4> initial = {
        {{101, "PLAY"}, {103, "OPTIONS"}, {102, "SLIDESHOW"}, {104, "QUIT"}}
    };
    static constexpr std::array<BackboxMenu::Item, 4> paused = {
        {{101, "RESUME"}, {103, "OPTIONS"}, {102, "SLIDESHOW"}, {104, "QUIT"}}
    };
    return session_available ? std::span(paused) : std::span(initial);
}
} // namespace
const Edition& selected_edition() {
    static constexpr std::array<BackboxMenu::Item, 6> options_menu = {
        {{207, "CONTROLS"}, {204, "TABLE VIEW"}, {208, "SOUND"}, {209, "FULLSCREEN"}, {202, "SCREEN FLIP"}, {206, "MAIN MENU"}}
    };
    static const Edition edition{
        .id = "enhanced",
        .title = "The Web Enhanced",
        .version = version,
        .rules_version = "enhanced-records-v2",
        .physics_version = "enhanced-physics-v4",
        .selectable_quality = false,
        .selectable_nudge = false,
        .pause_on_escape = true,
        .normalize_options = normalize_options,
        .options_menu = options_menu,
        .main_menu = main_menu,
        .default_options = options,
        .create_game = game,
        .create_nudge = nudge,
        .create_menu = menu,
        .consolidated_menu = true,
        .normalize_sound = enhanced::normalize_sound,
        .input_version = "timestamped-input-v2",
        .timestamped_input = true,
        .incremental_presentation = true,
        .sprite_antialiasing = true};
    return edition;
}
} // namespace web
