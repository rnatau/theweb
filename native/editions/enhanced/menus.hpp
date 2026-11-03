#pragma once
#include "../../host_settings.hpp"
#include "../../render/frontend_menus.hpp"

namespace web::enhanced {
class Menus final : public FrontendMenus {
public:
    Menus(const DotMatrixRenderer&, const FrontendAssets&, GameOptions&, HostSettings&, bool session_available);
    bool input(std::uint32_t, SoundSink*) override;
    bool activation_complete() const override;
    Action accept() override;
    Action escape() override;
    DotMatrixFrame step(const DotMatrixFrame&, std::int32_t) override;
    std::int32_t selected_id() const override;
    unsigned first_visible() const override;
    std::uint32_t counter() const override;
    std::int32_t activation_remaining() const override;
    bool capture_key(unsigned) override;
    bool capture_gamepad(std::uint64_t) override;
    bool capturing() const override {
        return capture_ >= 0;
    }
    unsigned page() const override {
        return page_;
    }

private:
    enum Page : unsigned {
        main = 0,
        options = 1,
        flip = 3,
        policy = 4,
        camera = 6,
        controls = 7,
        keyboard,
        gamepad,
        sound,
        effects_volume,
        music_volume,
        fullscreen
    };
    const DotMatrixRenderer& renderer_;
    GameOptions& options_;
    HostSettings& settings_;
    bool session_available_;
    CameraMenu camera_;
    Page page_ = main;
    std::vector<std::string> labels_;
    std::unique_ptr<BackboxMenu> list_;
    int capture_ = -1;
    std::string feedback_;
    unsigned last_music_volume_ = 128;
    void open(Page, int selected = 0);
    void rebuild(int selected = 0);
};
} // namespace web::enhanced
