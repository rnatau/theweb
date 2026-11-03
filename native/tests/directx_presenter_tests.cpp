#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "../platform/win32/swap_presenter.hpp"
#include <iostream>
#include <cmath>

namespace {
web::Bitmap reference(const web::PresentationFrame& frame, int width, int height) {
    const auto view = frame.viewport;
    const auto& source = frame.screen;
    web::Bitmap result{width, height, {}};
    result.pixels.assign(std::size_t(width) * height, 0);
    const bool exact = web::integer_presentation(view, source.width, source.height);
    const auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, source.width - 1);
        y = std::clamp(y, 0, source.height - 1);
        const auto layout = frame.display_layout;
        if (frame.dots && x >= layout.x && y >= layout.y && x < layout.x + layout.width() && y < layout.y + layout.height())
            return frame.display_backdrop->pixels[std::size_t(y - layout.y) * layout.width() + x - layout.x];
        return source.pixels[std::size_t(y) * source.width + x];
    };
    for (int y = 0; y < view.height; ++y)
        for (int x = 0; x < view.width; ++x) {
            double sx = (x + .5) * source.width / view.width, sy = (y + .5) * source.height / view.height;
            std::uint32_t color = 0;
            if (exact)
                color = sample(int(sx), int(sy));
            else {
                sx -= .5;
                sy -= .5;
                const auto bx = int(std::floor(sx)), by = int(std::floor(sy));
                const auto fx = sx - bx, fy = sy - by;
                const std::array colors{sample(bx, by), sample(bx + 1, by), sample(bx, by + 1), sample(bx + 1, by + 1)};
                for (const auto shift : {0, 8, 16}) {
                    const auto component = [&](unsigned i) { return double((colors[i] >> shift) & 255); };
                    const double top = component(0) * (1 - fx) + component(1) * fx;
                    const double bottom = component(2) * (1 - fx) + component(3) * fx;
                    color |= std::uint32_t(std::lround(top * (1 - fy) + bottom * fy)) << shift;
                }
            }
            result.pixels[std::size_t(view.y + y) * width + view.x + x] = color;
        }
    if (frame.dots)
        web::draw_presented_dmd(result.pixels, width, height, source, frame.display_layout, *frame.dots);
    web::present_nudge(result.pixels, width, height, view, {MulDiv(frame.nudge.x, view.width, source.width), MulDiv(frame.nudge.y, view.height, source.height)});
    return result;
}
void compare(const web::Bitmap& actual, const web::Bitmap& expected, int tolerance) {
    if (actual.width != expected.width || actual.height != expected.height)
        throw std::runtime_error("GPU composition dimensions changed");
    for (std::size_t i = 0; i < expected.pixels.size(); ++i)
        for (const auto shift : {0, 8, 16})
            if (std::abs(int((actual.pixels[i] >> shift) & 255) - int((expected.pixels[i] >> shift) & 255)) > tolerance)
                throw std::runtime_error("GPU composition differs at pixel " + std::to_string(i) + ": " + std::to_string(actual.pixels[i]) + " / " + std::to_string(expected.pixels[i]));
}
unsigned composition(web::SwapPresenter& presenter, HWND window) {
    unsigned frames = 0;
    web::Bitmap source{320, 240, {}};
    source.pixels.resize(320 * 240);
    for (std::size_t i = 0; i < source.pixels.size(); ++i)
        source.pixels[i] = std::uint32_t(i * 7919) & 0xffffff;
    const web::DmdLayout layout{32, 16, 2, 1, 1};
    web::Bitmap backdrop{layout.width(), layout.height(), {}};
    backdrop.pixels.assign(std::size_t(backdrop.width) * backdrop.height, 0x173149);
    for (const auto size : {
             std::pair{320,  240 },
             {640,  480 },
             {853,  499 },
             {257,  173 },
             {1920, 1080}
    }) {
        const auto [width, height] = size;
        SetWindowPos(window, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        const auto viewport = web::presentation_viewport(source.width, source.height, width, height, web::PresentationScaling::fit);
        for (const bool crisp : {false, true})
            for (const auto offset : {
                     web::NudgeOffset{},
                     web::NudgeOffset{4, -3},
                     web::NudgeOffset{-4, 3}
            }) {
                const web::PresentationFrame frame{source, &backdrop, layout, viewport, crisp ? web::presented_dmd(viewport, source.width, source.height, layout) : std::nullopt, offset, {}};
                if (!presenter.present_table(window, width, height, frame, {}, false))
                    throw std::runtime_error("Native GPU composition is unavailable");
                compare(presenter.capture(), reference(frame, width, height), web::integer_presentation(viewport, source.width, source.height) ? 0 : 1);
                ++frames;
            }
    }
    // Local animation and sparse edits must never change stationary side rails.
    // Repeated extra renders also must not depend on dirty-region history.
    const int width = 853, height = 499;
    SetWindowPos(window, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    const auto viewport = web::presentation_viewport(source.width, source.height, width, height, web::PresentationScaling::fit);
    const web::PresentationFrame frame{source, &backdrop, layout, viewport, {}, {}, {}};
    web::Bitmap first;
    for (unsigned animation = 0; animation < 20; ++animation) {
        source.pixels[120 * 320 + 160 + animation / 2] ^= 0xffffff;
        presenter.present_table(window, width, height, frame, {}, false);
        const auto actual = presenter.capture();
        compare(actual, reference(frame, width, height), 1);
        if (!animation)
            first = actual;
        for (int y = 0; y < height; ++y)
            for (int x = 0; x < width; ++x)
                if (std::abs(x - width / 2) > width / 4 && actual.pixels[std::size_t(y) * width + x] != first.pixels[std::size_t(y) * width + x])
                    throw std::runtime_error("GPU animation changed stationary table-side brightness");
        ++frames;
    }
    // Diagnostics are opaque, unscaled client rectangles and remain fixed
    // while the table and crisp DMD move together during a nudge.
    std::array<std::uint32_t, 12> pixels;
    pixels.fill(0x204080);
    const web::PresentationLayer layer{
        {7, 9, 4, 3},
        pixels
    };
    presenter.present_table(window, width, height, frame, std::span(&layer, 1), false);
    auto expected = reference(frame, width, height);
    for (int y = 9; y < 12; ++y)
        for (int x = 7; x < 11; ++x)
            expected.pixels[std::size_t(y) * width + x] = pixels[0];
    compare(presenter.capture(), expected, 1);
    return frames + 1;
}
} // namespace

int run() {
    WNDCLASSW type{};
    type.lpfnWndProc = DefWindowProcW;
    type.hInstance = GetModuleHandleW(nullptr);
    type.lpszClassName = L"TheWebGpuTest";
    RegisterClassW(&type);
    const auto window = CreateWindowExW(0, type.lpszClassName, L"The Web GPU verification", WS_POPUP, 0, 0, 320, 240, nullptr, nullptr, type.hInstance, nullptr);
    if (!window)
        throw std::runtime_error("Cannot create hidden GPU test window");
    struct Window {
        HWND handle;
        ~Window() {
            DestroyWindow(handle);
        }
    } owner{window};
    auto presenter = web::make_swap_presenter();
    presenter->capture_frames(true);
    unsigned frames = 0;
    for (unsigned lifetime = 0; lifetime < 3; ++lifetime) {
        for (const auto dimensions : {
                 std::pair{320, 240},
                 std::pair{640, 480},
                 std::pair{127, 95 }
        }) {
            const auto [width, height] = dimensions;
            SetWindowPos(window, nullptr, 0, 0, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
            web::Bitmap image;
            image.width = width;
            image.height = height;
            image.pixels.resize(std::size_t(width) * height);
            for (unsigned change = 0; change < 3; ++change) {
                for (std::size_t i = 0; i < image.pixels.size(); ++i)
                    image.pixels[i] = std::uint32_t((i * 7919 + change * 41) & 0xffffff);
                presenter->present(window, width, height, image.pixels, false);
                const auto actual = presenter->capture();
                if (actual.width != width || actual.height != height || actual.pixels != image.pixels) {
                    const auto mismatch = std::mismatch(actual.pixels.begin(), actual.pixels.end(), image.pixels.begin());
                    const auto index = std::size_t(mismatch.first - actual.pixels.begin());
                    throw std::runtime_error("GPU back buffer differs at " + std::to_string(index) + ": " + std::to_string(actual.pixels.at(index)) + " / " + std::to_string(image.pixels.at(index)));
                }
                ++frames;
            }
        }
        presenter->reset();
    }
    std::cout << "PASS: " << frames << " complete GPU back buffers, resize and device recreation; exact RGB pixels\n";
    const auto composed = composition(*presenter, window);
    if (!presenter->frame_ready_handle())
        throw std::runtime_error("GPU readiness handle is unavailable");
    const auto ready = WaitForSingleObject(presenter->frame_ready_handle(), 2000);
    if (ready != WAIT_OBJECT_0)
        throw std::runtime_error("GPU readiness did not signal");
    presenter->frame_ready();
    if (!presenter->acquire_frame())
        throw std::runtime_error("Host-consumed GPU readiness was waited for twice");
    presenter->reset();
    std::cout << "PASS: " << composed << " native GPU compositions, CPU bilinear reference, crisp DMD, nudge, fixed diagnostics and stable table sides\n";
    return 0;
}

int main() {
    try {
        return run();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
