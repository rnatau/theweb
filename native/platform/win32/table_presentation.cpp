#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "table_presentation.hpp"
#include "diagnostic_notice.hpp"
#include <algorithm>

namespace web {
TablePresentation::TablePresentation(std::unique_ptr<SwapPresenter> swap) :
    swap_presenter(std::move(swap)) {}
void TablePresentation::submitted(std::chrono::steady_clock::time_point started) {
    const bool stalled = std::chrono::steady_clock::now() - started > std::chrono::milliseconds(100);
    slow_frames = stalled ? slow_frames + 1 : 0;
    if (slow_frames >= 3)
        throw std::runtime_error("DirectX frame submission exceeded 100 ms for three consecutive frames");
    gpu_presented = true;
}
void TablePresentation::failed(DiagnosticLog& reports, const std::exception& error) {
    swap_presenter->reset();
    gpu_failed = true;
    gpu_presented = false;
    reports.report("DirectX unavailable; using GDI", error.what());
}
bool TablePresentation::gpu_paint(HWND window, HDC target, RECT client, const PresentationFrame& frame, DiagnosticLog& reports) {
    if (!window || !swap_presenter || gpu_failed || (frame.overlay && frame.overlay_bounds.width <= 0))
        return false;
    std::vector<PresentationLayer> layers;
    const auto overlay = frame.overlay_bounds;
    if (frame.overlay && overlay.width > 0 && overlay.height > 0) {
        const auto dc = overlay_buffer.resize(target, overlay.width, overlay.height);
        const auto saved = SaveDC(dc);
        SetViewportOrgEx(dc, frame.viewport.x - overlay.x, frame.viewport.y - overlay.y, nullptr);
        frame.overlay(dc, frame.viewport.width);
        RestoreDC(dc, saved);
        layers.push_back({overlay, overlay_buffer.pixels()});
    }
    const auto notice = reports.notice();
    if (!notice.empty()) {
        const PresentationRect area{8, std::max(8L, client.bottom - 40), std::max(1L, client.right - 16), 32};
        const auto dc = notice_buffer.resize(target, area.width, area.height);
        const auto saved = SaveDC(dc);
        SetViewportOrgEx(dc, -area.x, -area.y, nullptr);
        draw_diagnostic_notice(dc, client, notice);
        RestoreDC(dc, saved);
        layers.push_back({area, notice_buffer.pixels()});
    }
    if (!layers.empty())
        GdiFlush();
    try {
        const auto started = std::chrono::steady_clock::now();
        if (!swap_presenter->present_table(window, client.right, client.bottom, frame, layers, IsWindowVisible(window)))
            return false;
        submitted(started);
        reports.clear("DirectX unavailable; using GDI");
        return true;
    } catch (const std::exception& error) {
        failed(reports, error);
        return false;
    }
}
void TablePresentation::paint(HWND window, HDC target, RECT client, const PresentationFrame& frame, DiagnosticLog& reports) {
    const auto& screen = frame.screen;

    const int width = client.right, height = client.bottom;
    if (width > 0 && height > 0) {
        if (gpu_paint(window, target, client, frame, reports))
            return;
        const auto view = frame.viewport;
        const bool exact = web::integer_presentation(view, screen.width, screen.height);
        const auto dots = frame.dots;
        const web::Bitmap* image = &screen;
        web::Bitmap artwork;
        if (dots) {
            // Filter the original artwork without filtering its dot matrix.
            artwork = screen;
            const auto area = frame.display_layout;
            const auto& backdrop = *frame.display_backdrop;
            for (int row = 0; row < backdrop.height; ++row)
                std::copy_n(backdrop.pixels.begin() + std::size_t(row) * backdrop.width, backdrop.width, artwork.pixels.begin() + std::size_t(area.y + row) * artwork.width + area.x);
            image = &artwork;
        }
        HDC buffer = presentation.resize(target, width, height);
        HDC artwork_dc = scaled_artwork.render(target, width, height, *image, view, exact);
        BitBlt(buffer, 0, 0, width, height, artwork_dc, 0, 0, SRCCOPY);
        if (dots) {
            GdiFlush(); // Finish GDI writes before touching the DIB's memory.
            web::draw_presented_dmd(presentation.pixels(), width, height, screen, frame.display_layout, *dots);
        }
        const auto offset = frame.nudge;
        if (offset != web::NudgeOffset{}) {
            GdiFlush();
            web::present_nudge(presentation.pixels(), width, height, view, {MulDiv(offset.x, view.width, screen.width), MulDiv(offset.y, view.height, screen.height)});
        }
        const auto saved = SaveDC(buffer);
        SetViewportOrgEx(buffer, view.x, view.y, nullptr);
        if (frame.overlay)
            frame.overlay(buffer, view.width);
        RestoreDC(buffer, saved);
        web::draw_diagnostic_notice(buffer, client, reports.notice());
        bool displayed = false;
        if (window && swap_presenter && !gpu_failed) {
            GdiFlush();
            reports.clear("DirectX unavailable; using GDI");
            try {
                const auto started = std::chrono::steady_clock::now();
                swap_presenter->present(window, width, height, presentation.pixels(), IsWindowVisible(window));
                submitted(started);
                displayed = true;
            } catch (const std::exception& error) {
                failed(reports, error);
                web::draw_diagnostic_notice(buffer, client, reports.notice());
            }
        }
        if (!displayed)
            BitBlt(target, 0, 0, width, height, buffer, 0, 0, SRCCOPY);
    }
}
void TablePresentation::reset_device() {
    if (swap_presenter)
        swap_presenter->reset();
    gpu_failed = gpu_presented = false;
    slow_frames = 0;
}
bool TablePresentation::acquire_frame(DiagnosticLog& reports) {
    if (!swap_presenter || gpu_failed)
        return true;
    try {
        return swap_presenter->acquire_frame();
    } catch (const std::exception& error) {
        failed(reports, error);
        return true;
    }
}
void TablePresentation::reset() {
    reset_device();
    presentation.reset();
    overlay_buffer.reset();
    notice_buffer.reset();
}
} // namespace web
