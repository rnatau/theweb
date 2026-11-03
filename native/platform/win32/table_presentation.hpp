#pragma once
#include "swap_presenter.hpp"
#include "scaled_artwork.hpp"
#include "../diagnostic_log.hpp"
#include "../../render/nudge_feedback.hpp"
#include <functional>

namespace web {
// All window composition resources, device recovery and capture/GDI fallback
// live here. Painting receives a read-only image and never advances the game.
class TablePresentation {
public:
    explicit TablePresentation(std::unique_ptr<SwapPresenter> = make_swap_presenter());
    void paint(HWND, HDC, RECT, const PresentationFrame&, DiagnosticLog&);
    void reset_device();
    void reset();
    bool acquire_frame(DiagnosticLog&);
    HANDLE frame_ready_handle() const {
        return swap_presenter && gpu_presented && !gpu_failed ? swap_presenter->frame_ready_handle() : nullptr;
    }
    void frame_ready() {
        swap_presenter->frame_ready();
    }
    bool gpu_active() const {
        return gpu_presented;
    }
    bool using_fallback() const {
        return gpu_failed;
    }

private:
    std::unique_ptr<SwapPresenter> swap_presenter;
    bool gpu_failed = false, gpu_presented = false;
    unsigned slow_frames = 0;
    PresentationBuffer presentation;
    ScaledArtwork scaled_artwork;
    PresentationBuffer overlay_buffer, notice_buffer;
    bool gpu_paint(HWND, HDC, RECT, const PresentationFrame&, DiagnosticLog&);
    void submitted(std::chrono::steady_clock::time_point);
    void failed(DiagnosticLog&, const std::exception&);
};
} // namespace web
