#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../platform/win32/scaled_artwork.hpp"
#include <cstdio>
#include <stdexcept>

int main() {
    try {
        web::ScaledArtwork cached;
        web::PresentationBuffer reference;
        unsigned frames = 0;
        for (const auto size : {
                 std::pair{3440, 1440},
                 {1920, 1080},
                 {1024, 768 },
                 {640,  480 },
                 {137,  93  },
                 {2160, 3840}
        })
            for (const auto source_size : {
                     std::pair{1024, 768},
                     {640,  480},
                     {800,  600}
            }) {
                web::Bitmap source;
                source.width = source_size.first;
                source.height = source_size.second;
                source.pixels.resize(std::size_t(source.width) * source.height);
                for (std::size_t i = 0; i < source.pixels.size(); ++i)
                    source.pixels[i] = std::uint32_t(i * 2654435761u) & 0xffffff;
                for (const auto scaling : {web::PresentationScaling::fit, web::PresentationScaling::integer}) {
                    const auto view = web::presentation_viewport(source.width, source.height, size.first, size.second, scaling);
                    const bool exact = web::integer_presentation(view, source.width, source.height);
                    const auto dc = reference.resize(nullptr, size.first, size.second);
                    RECT client{0, 0, size.first, size.second};
                    FillRect(dc, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
                    BITMAPINFO info{};
                    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    info.bmiHeader.biWidth = source.width;
                    info.bmiHeader.biHeight = -source.height;
                    info.bmiHeader.biPlanes = 1;
                    info.bmiHeader.biBitCount = 32;
                    SetStretchBltMode(dc, exact ? COLORONCOLOR : HALFTONE);
                    SetBrushOrgEx(dc, 0, 0, nullptr);
                    for (int frame = 0; frame < 18; ++frame) {
                        // Changes on tile boundaries, isolated corners, broad
                        // redraws and unchanged paints exercise filter halos.
                        if (frame != 1) {
                            const int x = (frame * 31) % source.width, y = (frame * 33) % source.height;
                            source.pixels[std::size_t(y) * source.width + x] ^= 0xffffff;
                            source.pixels[frame % 2 ? 0 : source.pixels.size() - 1] ^= 0xffffff;
                            if (frame == 9)
                                for (auto& pixel : source.pixels)
                                    pixel ^= 0x3a517f;
                        }
                        cached.render(nullptr, size.first, size.second, source, view, exact);
                        StretchDIBits(dc, view.x, view.y, view.width, view.height, 0, 0, source.width, source.height, source.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
                        // Compare through a separate DIB, also checking that
                        // callers can draw overlays without touching the cache.
                        web::PresentationBuffer result;
                        const auto result_dc = result.resize(nullptr, size.first, size.second);
                        BitBlt(result_dc, 0, 0, size.first, size.second, cached.render(nullptr, size.first, size.second, source, view, exact), 0, 0, SRCCOPY);
                        GdiFlush();
                        const auto expected = reference.pixels(), actual = result.pixels();
                        for (std::size_t i = 0; i < expected.size(); ++i)
                            if ((expected[i] & 0xffffff) != (actual[i] & 0xffffff)) {
                                std::fprintf(stderr, "Mismatch %dx%d -> %dx%d frame %d pixel %zu\n", source.width, source.height, size.first, size.second, frame, i);
                                throw std::runtime_error("Cached scaling changed a pixel");
                            }
                        ++frames;
                    }
                }
            }
        std::printf("PASS: %u complete cached/full GDI frames, resize, filtering, corners and unchanged paints\n", frames);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
