#pragma once
#include <windows.h>
#include <cstdint>
#include <span>
#include <stdexcept>

namespace web {
// Reusable client-sized DIB. Compose a complete frame here before the single
// visible BitBlt, so background clears cannot appear between image updates.
class PresentationBuffer {
    HDC dc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
    std::uint32_t* pixels_ = nullptr;
    int width_ = 0, height_ = 0;

public:
    PresentationBuffer() = default;
    PresentationBuffer(const PresentationBuffer&) = delete;
    PresentationBuffer& operator=(const PresentationBuffer&) = delete;
    ~PresentationBuffer() {
        reset();
    }
    void reset() {
        if (previous_)
            SelectObject(dc_, previous_);
        if (bitmap_)
            DeleteObject(bitmap_);
        if (dc_)
            DeleteDC(dc_);
        dc_ = nullptr;
        bitmap_ = nullptr;
        previous_ = nullptr;
        pixels_ = nullptr;
        width_ = 0;
        height_ = 0;
    }
    HDC resize(HDC target, int width, int height) {
        if (dc_ && width == width_ && height == height_)
            return dc_;
        reset();
        dc_ = CreateCompatibleDC(target);
        if (dc_) {
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = width;
            info.bmiHeader.biHeight = -height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            void* pixels = nullptr;
            bitmap_ = CreateDIBSection(dc_, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
            pixels_ = static_cast<std::uint32_t*>(pixels);
        }
        if (!bitmap_) {
            reset();
            throw std::runtime_error("Cannot allocate presentation buffer");
        }
        auto previous = SelectObject(dc_, bitmap_);
        if (!previous || previous == HGDI_ERROR) {
            reset();
            throw std::runtime_error("Cannot select presentation buffer");
        }
        previous_ = previous;
        width_ = width;
        height_ = height;
        return dc_;
    }
    std::span<std::uint32_t> pixels() {
        return {pixels_, std::size_t(width_) * height_};
    }
};
} // namespace web
