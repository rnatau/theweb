#pragma once
#include "dot_matrix.hpp"
#include "frontend_assets.hpp"
#include "menu_selection.hpp"

namespace web {
// Original MENU table-view list (0x4029e8), using six transparent 32x24 icons.
class CameraMenu {
public:
    explicit CameraMenu(const FrontendAssets& assets);
    void next();
    void previous();
    void select(std::int32_t id);
    void remember() {
        selection_.remember();
    }
    void restore();
    bool input(std::uint32_t edges, SoundSink* sounds = nullptr);
    void activate() {
        activation_remaining_ = 4;
    }
    bool activation_complete() const {
        return activation_remaining_ < 1;
    }
    DotMatrixFrame step(const DotMatrixFrame& backdrop, std::int32_t elapsed = 1);
    const MenuSelection& selection() const {
        return selection_;
    }
    unsigned first_visible() const {
        return first_visible_;
    }
    std::uint32_t counter() const {
        return counter_;
    }
    std::int32_t activation_remaining() const {
        return activation_remaining_;
    }

private:
    std::array<Bytes, FrontendAssets::view_count> icons_;
    MenuSelection selection_{
        {401, 402, 403, 404, 405, 406}
    };
    unsigned first_visible_ = 0;
    std::uint32_t counter_ = 0;
    std::int32_t activation_remaining_ = 0;
    void fit_selection();
};
} // namespace web
