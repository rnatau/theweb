#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../platform/win32/table_presentation.hpp"
#include <iostream>
#include <thread>

namespace {
class FailedDevice final : public web::SwapPresenter {
public:
    unsigned attempts = 0, resets = 0;
    bool fail = true;
    void present(HWND, int, int, std::span<const std::uint32_t>, bool) override {
        ++attempts;
        if (fail)
            throw std::runtime_error("synthetic device loss");
        std::this_thread::sleep_for(std::chrono::milliseconds(110));
    }
    void capture_frames(bool) override {}
    web::Bitmap capture() override {
        return {};
    }
    void reset() override {
        ++resets;
    }
    bool acquire_frame() override {
        if (fail)
            throw std::runtime_error("synthetic readiness loss");
        return true;
    }
};
class NativeDevice final : public web::SwapPresenter {
public:
    unsigned frames = 0;
    bool fail = false;
    void present(HWND, int, int, std::span<const std::uint32_t>, bool) override {
        throw std::runtime_error("Native presentation entered the screen-sized upload path");
    }
    bool present_table(HWND, int width, int height, const web::PresentationFrame&, std::span<const web::PresentationLayer> layers, bool) override {
        ++frames;
        if (fail)
            throw std::runtime_error("synthetic native composition failure");
        if (width != 128 || height != 96 || layers.size() != 1 || layers[0].bounds != web::PresentationRect{8, 8, 24, 16})
            throw std::runtime_error("Native presentation did not retain the small diagnostic rectangle");
        for (const auto pixel : layers[0].pixels)
            if ((pixel & 0xffffff) != 0x010203)
                throw std::runtime_error("Native diagnostic rectangle contains stale pixels");
        return true;
    }
    void capture_frames(bool) override {}
    web::Bitmap capture() override {
        return {};
    }
    void reset() override {}
};
} // namespace
int main() {
    auto failed = std::make_unique<FailedDevice>();
    auto* device = failed.get();
    web::TablePresentation presentation(std::move(failed));
    web::PresentationBuffer destination;
    HDC reference = GetDC(nullptr);
    const auto dc = destination.resize(reference, 128, 96);
    ReleaseDC(nullptr, reference);
    web::Bitmap source;
    source.width = source.height = 4;
    source.pixels.assign(16, 0x00010203);
    const web::PresentationFrame frame{
        source,
        nullptr,
        {},
        {0, 0, 128, 96},
        {},
        {},
        {}
    };
    web::DiagnosticLog reports;
    presentation.paint(reinterpret_cast<HWND>(1), dc, {0, 0, 128, 96}, frame, reports);
    if (presentation.gpu_active() || reports.notice() != "DirectX unavailable; using GDI" || device->attempts != 1 || device->resets != 1)
        throw std::runtime_error("DirectX failure did not select GDI and report its notice");
    const auto pixel = GetPixel(dc, 0, 0);
    if (GetRValue(pixel) != 1 || GetGValue(pixel) != 2 || GetBValue(pixel) != 3)
        throw std::runtime_error("GDI fallback did not paint the completed canvas");
    const auto notice_pixel = GetPixel(dc, 10, 57);
    if (GetRValue(notice_pixel) != 32 || GetGValue(notice_pixel) != 20 || GetBValue(notice_pixel) != 0)
        throw std::runtime_error("Fallback notice was not painted visibly");
    presentation.paint(reinterpret_cast<HWND>(1), dc, {0, 0, 128, 96}, frame, reports);
    if (device->attempts != 1)
        throw std::runtime_error("Failed device was retried on every paint");
    presentation.reset_device();
    presentation.paint(reinterpret_cast<HWND>(1), dc, {0, 0, 128, 96}, frame, reports);
    if (device->attempts != 2)
        throw std::runtime_error("Display/device reset did not permit recovery");
    device->fail = false;
    presentation.reset_device();
    for (unsigned i = 0; i < 3; ++i)
        presentation.paint(reinterpret_cast<HWND>(1), dc, {0, 0, 128, 96}, frame, reports);
    if (!presentation.using_fallback() || presentation.gpu_active())
        throw std::runtime_error("Persistently stalled adapter did not select the safe fallback");
    presentation.reset_device();
    device->fail = true;
    const auto attempts = device->attempts;
    if (!presentation.acquire_frame(reports) || !presentation.using_fallback() || presentation.gpu_active())
        throw std::runtime_error("Readiness failure did not allow immediate GDI fallback");
    presentation.paint(reinterpret_cast<HWND>(1), dc, {0, 0, 128, 96}, frame, reports);
    if (device->attempts != attempts || reports.notice() != "DirectX unavailable; using GDI")
        throw std::runtime_error("Readiness failure was retried or its notice was lost");
    auto native = std::make_unique<NativeDevice>();
    auto* gpu = native.get();
    web::TablePresentation direct(std::move(native));
    web::DiagnosticLog native_reports;
    auto with_overlay = frame;
    with_overlay.overlay_bounds = {8, 8, 24, 16};
    with_overlay.overlay = [](HDC dc, int) {
        RECT area{8, 8, 32, 24};
        const auto brush = CreateSolidBrush(RGB(1, 2, 3));
        FillRect(dc, &area, brush);
        DeleteObject(brush);
    };
    direct.paint(reinterpret_cast<HWND>(1), dc, {0, 0, 128, 96}, with_overlay, native_reports);
    if (!direct.gpu_active() || direct.using_fallback() || gpu->frames != 1)
        throw std::runtime_error("Native GPU presentation was not selected");
    gpu->fail = true;
    direct.paint(reinterpret_cast<HWND>(1), dc, {0, 0, 128, 96}, with_overlay, native_reports);
    if (direct.gpu_active() || !direct.using_fallback() || native_reports.notice() != "DirectX unavailable; using GDI")
        throw std::runtime_error("Native composition failure did not select visible GDI fallback");
    std::cout << "PASS: DirectX loss/stall, visible notice, exact GDI fallback and bounded retry\n";
}
