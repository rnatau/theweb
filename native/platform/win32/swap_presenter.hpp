#pragma once
#include "../../web.hpp"
#include "../../render/nudge_feedback.hpp"
#include <windows.h>
#include <functional>
#include <memory>

namespace web {
struct PresentationFrame {
    const Bitmap& screen;
    const Bitmap* display_backdrop;
    DmdLayout display_layout;
    PresentationRect viewport;
    std::optional<PresentedDmd> dots;
    NudgeOffset nudge;
    std::function<void(HDC, int)> overlay;
    PresentationRect overlay_bounds{};
};
struct PresentationLayer {
    PresentationRect bounds;
    std::span<const std::uint32_t> pixels;
};
// Edition-linked Windows service. Classic supplies no swap chain and keeps GDI.
// Enhanced uploads the completed, pixel-verified canvas to an opaque flip chain.
class SwapPresenter {
public:
    virtual ~SwapPresenter() = default;
    virtual void present(HWND, int width, int height, std::span<const std::uint32_t>, bool synchronize = true) = 0;
    virtual void capture_frames(bool) = 0;
    virtual Bitmap capture() = 0; // Actual last GPU back buffer, for device tests.
    virtual void reset() = 0;
    // Render the native canvas directly; diagnostic layers are small opaque
    // rectangles in client coordinates. Unsupported services retain GDI.
    virtual bool present_table(HWND, int, int, const PresentationFrame&, std::span<const PresentationLayer>, bool = true) {
        return false;
    }
    virtual HANDLE frame_ready_handle() const {
        return nullptr;
    }
    // The host's wait consumed the auto-reset readiness signal.
    virtual void frame_ready() {}
    // Poll before simulation/composition, so the submitted image uses current
    // input and physics rather than waiting after it has already been drawn.
    virtual bool acquire_frame() {
        return true;
    }
};
std::unique_ptr<SwapPresenter> make_swap_presenter();
} // namespace web
