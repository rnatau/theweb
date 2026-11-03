#pragma once
#include "presentation_buffer.hpp"
#include "../../render/presentation.hpp"
#include <algorithm>
#include <cstring>

namespace web {
// Retain filtered artwork independently of the crisp DMD, nudge and overlay.
// Clipping the original full-image stretch preserves its sampling coordinates.
class ScaledArtwork {
    struct Region {
        HRGN handle = CreateRectRgn(0, 0, 0, 0);
        Region() {
            if (!handle)
                throw std::runtime_error("Cannot allocate artwork dirty region");
        }
        ~Region() {
            DeleteObject(handle);
        }
    };
    PresentationBuffer buffer_;
    Region dirty_, tile_;
    Bitmap previous_;
    PresentationRect view_;
    int width_ = 0, height_ = 0;
    bool exact_ = false;

public:
    HDC render(HDC target, int width, int height, const Bitmap& source, PresentationRect view, bool exact) {
        const bool full = width != width_ || height != height_ || view != view_ || exact != exact_ || source.width != previous_.width || source.height != previous_.height;
        HDC dc = buffer_.resize(target, width, height);
        if (!SetRectRgn(dirty_.handle, 0, 0, 0, 0))
            throw std::runtime_error("Cannot reset artwork dirty region");
        bool changed = full;
        if (full) {
            RECT client{0, 0, width, height};
            FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        } else {
            constexpr int tile_size = 32;
            // Include neighboring filter samples, also when downscaling.
            const int halo_x = 2 + (source.width + view.width - 1) / view.width;
            const int halo_y = 2 + (source.height + view.height - 1) / view.height;
            for (int y = 0; y < source.height; y += tile_size)
                for (int x = 0; x < source.width; x += tile_size) {
                    const int right = std::min(x + tile_size, source.width), bottom = std::min(y + tile_size, source.height);
                    bool different = false;
                    for (int row = y; row < bottom && !different; ++row) {
                        const auto offset = std::size_t(row) * source.width + x;
                        different = std::memcmp(source.pixels.data() + offset, previous_.pixels.data() + offset, std::size_t(right - x) * sizeof(std::uint32_t)) != 0;
                    }
                    if (!different)
                        continue;
                    const int left_out = view.x + int(std::int64_t(std::max(0, x - halo_x)) * view.width / source.width);
                    const int top_out = view.y + int(std::int64_t(std::max(0, y - halo_y)) * view.height / source.height);
                    const int right_out = view.x + int((std::int64_t(std::min(source.width, right + halo_x)) * view.width + source.width - 1) / source.width);
                    const int bottom_out = view.y + int((std::int64_t(std::min(source.height, bottom + halo_y)) * view.height + source.height - 1) / source.height);
                    if (!SetRectRgn(tile_.handle, left_out, top_out, right_out, bottom_out) || CombineRgn(dirty_.handle, dirty_.handle, tile_.handle, RGN_OR) == ERROR)
                        throw std::runtime_error("Cannot combine artwork dirty regions");
                    changed = true;
                }
        }
        if (changed) {
            if (!full && SelectClipRgn(dc, dirty_.handle) == ERROR)
                throw std::runtime_error("Cannot clip changed artwork");
            BITMAPINFO info{};
            info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth = source.width;
            info.bmiHeader.biHeight = -source.height;
            info.bmiHeader.biPlanes = 1;
            info.bmiHeader.biBitCount = 32;
            SetStretchBltMode(dc, exact ? COLORONCOLOR : HALFTONE);
            SetBrushOrgEx(dc, 0, 0, nullptr);
            StretchDIBits(dc, view.x, view.y, view.width, view.height, 0, 0, source.width, source.height, source.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
            SelectClipRgn(dc, nullptr);
            // GDI may defer reading the source until its batch is flushed.
            GdiFlush();
            previous_.width = source.width;
            previous_.height = source.height;
            previous_.pixels = source.pixels;
        }
        width_ = width;
        height_ = height;
        view_ = view;
        exact_ = exact;
        return dc;
    }
};
} // namespace web
