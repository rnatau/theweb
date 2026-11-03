#include "../../edition.hpp"
#include "../../gameplay.hpp"

namespace web {
namespace {
constexpr std::string_view version = "1.52p";
class OriginalNudge final : public NudgePresentation {
public:
    bool trigger(Bitmap& front, unsigned variant, std::uint32_t) override {
        nudge_feedback(front, variant);
        return true;
    }
    void advance(unsigned) override {}
    void reset() override {}
    NudgeOffset offset(unsigned) const override {
        return {};
    }
};
GameOptions options() {
    return GameOptions{};
}
void retain_options(GameOptions&) {}
std::span<const BackboxMenu::Item> main_menu(bool) {
    return {}; // Retain the original menu list in every state.
}
std::unique_ptr<GameSession> game(const std::filesystem::path& assets) {
    auto result = std::make_unique<PinballGame>(assets);
    result->startup_version(version);
    return result;
}
std::unique_ptr<NudgePresentation> nudge() {
    return std::make_unique<OriginalNudge>();
}
} // namespace
const Edition& selected_edition() {
    static const Edition edition{
        .id = "classic",
        .title = "The Web",
        .version = version,
        .rules_version = "windows-1.50P",
        .physics_version = "windows-1.50P-integer",
        .selectable_quality = true,
        .selectable_nudge = true,
        .pause_on_escape = false,
        .normalize_options = retain_options,
        .options_menu = {},
        .main_menu = main_menu,
        .default_options = options,
        .create_game = game,
        .create_nudge = nudge};
    return edition;
}
} // namespace web
