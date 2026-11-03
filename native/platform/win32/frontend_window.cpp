#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include <dbt.h>
#include "frontend_window.hpp"
#include "../diagnostic_log.hpp"
#include "diagnostic_notice.hpp"
#include "presentation_buffer.hpp"
#include "gamepad_input.hpp"
#include "../../gameplay.hpp"
#include "../../render/presentation.hpp"
#include <algorithm>
#include <chrono>
#include <string_view>
#include <system_error>

namespace web {
struct FrontendWindow::Impl {
    DiagnosticLog* reports = nullptr;
    static constexpr DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
    FrontendWindow& controller;
    Bitmap canvas;
    PresentationBuffer presentation;
    std::wstring class_name, title;
    HWND window = nullptr;
    HWND owner_window = nullptr;
    SimulationClock clock;
    std::chrono::steady_clock::time_point last;
    bool focused = true, moving = false, fitting = false, fullscreen = false;
    int scale = 1, width = 0, height = 0;
    HMONITOR monitor = nullptr;
    bool active = false;
    std::unique_ptr<GamepadInput> gamepad;
    std::uint64_t previous_pad = 0;
    bool pad_armed = false;
    Impl(FrontendWindow& controller, Bitmap image, std::wstring name, std::wstring caption) :
        controller(controller),
        canvas(std::move(image)),
        class_name(std::move(name)),
        title(std::move(caption)) {}
    void reset_clock() {
        clock.reset();
        last = std::chrono::steady_clock::now();
    }
    void update_activity() {
        const bool current = window && focused && !moving && !IsIconic(window);
        if (current != active) {
            active = current;
            controller.activity(active);
        }
    }
    void fit(HMONITOR target = nullptr, UINT dpi = 0) {
        monitor = target ? target : MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (!GetMonitorInfoW(monitor, &info))
            throw std::runtime_error("Cannot find frontend monitor");
        if (fullscreen) {
            width = info.rcMonitor.right - info.rcMonitor.left;
            height = info.rcMonitor.bottom - info.rcMonitor.top;
            scale = std::max(1, std::min(width / canvas.width, height / canvas.height));
            fitting = true;
            const auto result = SetWindowPos(window, nullptr, info.rcMonitor.left, info.rcMonitor.top, width, height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
            fitting = false;
            if (!result)
                throw std::runtime_error("Cannot fit fullscreen frontend");
            InvalidateRect(window, nullptr, FALSE);
            return;
        }
        if (!dpi)
            dpi = GetDpiForWindow(window);
        RECT frame{};
        if (!AdjustWindowRectExForDpi(&frame, style, FALSE, 0, dpi))
            throw std::runtime_error("Cannot calculate frontend window size");
        const int border_width = frame.right - frame.left, border_height = frame.bottom - frame.top;
        const int available_width = info.rcWork.right - info.rcWork.left, available_height = info.rcWork.bottom - info.rcWork.top;
        scale = canvas.width * 2 + border_width <= available_width && canvas.height * 2 + border_height <= available_height ? 2 : 1;
        width = canvas.width * scale + border_width;
        height = canvas.height * scale + border_height;
        const int x = info.rcWork.left + std::max(0, (available_width - width) / 2), y = info.rcWork.top + std::max(0, (available_height - height) / 2);
        fitting = true;
        const auto result = SetWindowPos(window, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        fitting = false;
        if (!result)
            throw std::runtime_error("Cannot size frontend window");
        InvalidateRect(window, nullptr, FALSE);
    }
    void step() {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(now - last).count();
        last = now;
        if (!focused || moving || IsIconic(window)) {
            clock.reset();
            previous_pad = 0;
            pad_armed = false;
            return;
        }
        if (gamepad) {
            const auto reading = gamepad->poll(window);
            if (!reading.buttons)
                pad_armed = true;
            const auto edges = pad_armed ? reading.buttons & ~previous_pad : 0;
            previous_pad = reading.buttons;
            if (edges && !controller.capture_gamepad(edges)) {
                if (edges & (pad_button::up | pad_button::left | pad_button::stick_up | pad_button::stick_left | pad_button::lb))
                    controller.key(VK_LEFT);
                else if (edges & (pad_button::down | pad_button::right | pad_button::stick_down | pad_button::stick_right | pad_button::rb))
                    controller.key(VK_RIGHT);
                else if (edges & (pad_button::a | pad_button::start))
                    controller.key(VK_RETURN);
                else if (edges & pad_button::b)
                    controller.key(VK_ESCAPE);
            }
            if (!window || !IsWindow(window))
                return;
        }
        const auto count = clock.advance(std::uint64_t(elapsed));
        for (unsigned i = 0; i < count && window; ++i)
            controller.tick();
    }
    void toggle_fullscreen(LPARAM key_flags) {
        wchar_t owner_class[80]{};
        GetClassNameW(owner_window, owner_class, int(std::size(owner_class)));
        const std::wstring_view name(owner_class);
        if (name == L"TheWebNativePreview" || name.starts_with(L"TheWebOriginal")) {
            // Keep the retained table and nested original frontends in the same
            // host mode. Native settings dialogs retain their modal geometry.
            SendMessageW(owner_window, WM_SYSKEYDOWN, VK_RETURN, key_flags);
            fullscreen = !(GetWindowLongPtrW(owner_window, GWL_STYLE) & WS_CAPTION);
        } else {
            fullscreen = !fullscreen;
        }
        fitting = true;
        SetLastError(0);
        const auto previous = SetWindowLongPtrW(window, GWL_STYLE, (fullscreen ? WS_POPUP : style) | (GetWindowLongPtrW(window, GWL_STYLE) & (WS_VISIBLE | WS_DISABLED | WS_MINIMIZE)));
        const auto error = GetLastError();
        fitting = false;
        if (!previous && error)
            throw std::system_error(int(error), std::system_category(), "Cannot change frontend fullscreen style");
        fit();
        reset_clock();
        if (IsWindowVisible(window) && IsWindowEnabled(window))
            SetFocus(window);
    }
    void paint_to(HDC dc) {
        if (!dc)
            return;
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = canvas.width;
        info.bmiHeader.biHeight = -canvas.height;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        RECT client{};
        if (!GetClientRect(window, &client) || client.right <= 0 || client.bottom <= 0)
            return;
        const auto view = presentation_viewport(canvas.width, canvas.height, client.right, client.bottom, PresentationScaling::integer);
        const auto buffer = presentation.resize(dc, client.right, client.bottom);
        SetStretchBltMode(buffer, COLORONCOLOR);
        FillRect(buffer, &client, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
        StretchDIBits(buffer, view.x, view.y, view.width, view.height, 0, 0, canvas.width, canvas.height, canvas.pixels.data(), &info, DIB_RGB_COLORS, SRCCOPY);
        if (reports)
            draw_diagnostic_notice(buffer, client, reports->notice());
        BitBlt(dc, 0, 0, client.right, client.bottom, buffer, 0, 0, SRCCOPY);
    }
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
        auto* self = reinterpret_cast<Impl*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<Impl*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            self->window = window;
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (!self)
            return DefWindowProcW(window, message, w, l);
        try {
            switch (message) {
                case WM_SYSKEYDOWN:
                    if (w == VK_RETURN && (l & (1LL << 29))) {
                        if (!(l & (1LL << 30)))
                            self->toggle_fullscreen(l);
                        return 0;
                    }
                    break;
                case WM_SYSKEYUP:
                    if (w == VK_RETURN)
                        return 0;
                    break;
                case WM_SYSCHAR:
                    if (w == VK_RETURN)
                        return 0;
                    break;
                case WM_TIMER:
                    self->step();
                    return 0;
                case WM_DEVICECHANGE:
                    if (self->gamepad && w == DBT_DEVNODES_CHANGED)
                        self->gamepad->devices_changed();
                    return TRUE;
                case WM_PAINT: {
                    PAINTSTRUCT paint{};
                    const auto dc = BeginPaint(window, &paint);
                    try {
                        self->paint_to(dc);
                    } catch (...) {
                        EndPaint(window, &paint);
                        throw;
                    }
                    EndPaint(window, &paint);
                    return 0;
                }
                case WM_PRINTCLIENT:
                    self->paint_to(reinterpret_cast<HDC>(w));
                    return 0;
                case WM_ERASEBKGND:
                    return 1;
                case WM_KEYDOWN:
                    if (!(l & (1LL << 30))) {
                        unsigned key = unsigned(w);
                        if (key == VK_SHIFT)
                            key = MapVirtualKeyW((unsigned(l) >> 16) & 255, MAPVK_VSC_TO_VK_EX);
                        else if (key == VK_CONTROL)
                            key = (l & (1LL << 24)) ? VK_RCONTROL : VK_LCONTROL;
                        self->controller.key(key);
                    }
                    return 0;
                case WM_KILLFOCUS:
                    self->focused = false;
                    self->controller.blur();
                    self->reset_clock();
                    self->update_activity();
                    return 0;
                case WM_SETFOCUS:
                    self->focused = true;
                    self->reset_clock();
                    self->update_activity();
                    return 0;
                case WM_ENTERSIZEMOVE:
                    self->moving = true;
                    self->controller.blur();
                    self->reset_clock();
                    self->update_activity();
                    return 0;
                case WM_EXITSIZEMOVE:
                    self->moving = false;
                    if (MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) != self->monitor)
                        self->fit();
                    self->reset_clock();
                    self->update_activity();
                    return 0;
                case WM_SIZE:
                    self->update_activity();
                    break;
                case WM_GETMINMAXINFO:
                    if (!self->fitting && self->width) {
                        auto& limits = *reinterpret_cast<MINMAXINFO*>(l);
                        limits.ptMinTrackSize = {self->width, self->height};
                        limits.ptMaxTrackSize = limits.ptMinTrackSize;
                        return 0;
                    }
                    break;
                case WM_WINDOWPOSCHANGING:
                    if (!self->fitting && self->width && !IsIconic(window) && !(GetWindowLongPtrW(window, GWL_STYLE) & WS_MINIMIZE)) {
                        auto& position = *reinterpret_cast<WINDOWPOS*>(l);
                        if (!(position.flags & SWP_NOSIZE)) {
                            position.cx = self->width;
                            position.cy = self->height;
                        }
                    }
                    break;
                case WM_WINDOWPOSCHANGED:
                    if (!self->fitting && !self->moving && self->width && !IsIconic(window) && MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST) != self->monitor)
                        self->fit();
                    break;
                case WM_DPICHANGED:
                    if (!self->fitting && self->width)
                        self->fit(MonitorFromRect(reinterpret_cast<RECT*>(l), MONITOR_DEFAULTTONEAREST), LOWORD(w));
                    return 0;
                case WM_DISPLAYCHANGE:
                case WM_SETTINGCHANGE:
                    if (!self->fitting && self->width && (message == WM_DISPLAYCHANGE || w == SPI_SETWORKAREA))
                        self->fit();
                    return 0;
                case WM_DESTROY:
                    KillTimer(window, 1);
                    return 0;
                case WM_NCDESTROY:
                    self->window = nullptr;
                    break;
                default:
                    if (message > WM_APP && message < WM_APP + 32) {
                        const auto id = message - WM_APP;
                        if (const auto value = self->controller.probe(id))
                            return LRESULT(*value);
                        if (id == 3 || id == 6)
                            return self->scale;
                    }
                    break;
            }
        } catch (const std::exception& error) {
            MessageBoxA(window, error.what(), "The Web frontend", MB_ICONERROR);
            DestroyWindow(window);
            return 0;
        }
        return DefWindowProcW(window, message, w, l);
    }
    void run(HWND owner) {
        owner_window = owner;
        if (owner && GetPropW(owner, L"TheWebGamepadMenus"))
            controller.gamepad(true);
        if (canvas.width <= 0 || canvas.height <= 0 || canvas.pixels.size() != std::size_t(canvas.width) * canvas.height)
            throw std::runtime_error("Frontend canvas has invalid dimensions");
        WNDCLASSW klass{};
        klass.lpfnWndProc = window_proc;
        klass.hInstance = GetModuleHandleW(nullptr);
        klass.lpszClassName = class_name.c_str();
        klass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        if (!RegisterClassW(&klass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::runtime_error("Cannot register frontend window");
        // A home frontend replaces a hidden table but still needs its own
        // taskbar button. Keep modal ownership for focus and monitor placement.
        fullscreen = owner && !(GetWindowLongPtrW(owner, GWL_STYLE) & WS_CAPTION);
        const auto extended_style = owner && !IsWindowVisible(owner) ? WS_EX_APPWINDOW : 0;
        const auto created = CreateWindowExW(
            extended_style,
            klass.lpszClassName,
            title.c_str(),
            fullscreen ? WS_POPUP : style,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            canvas.width,
            canvas.height,
            owner,
            nullptr,
            klass.hInstance,
            this);
        if (!created)
            throw std::runtime_error("Cannot open frontend window");
        if (gamepad)
            SetPropW(created, L"TheWebGamepadMenus", reinterpret_cast<HANDLE>(1));
        struct Modal {
            HWND window, owner;
            bool enabled;
            ~Modal() {
                if (IsWindow(window))
                    DestroyWindow(window);
                if (enabled && IsWindow(owner)) {
                    EnableWindow(owner, TRUE);
                    SetActiveWindow(owner);
                    SetFocus(owner);
                }
            }
        } modal{created, owner, IsWindowEnabled(owner) != FALSE};
        fit(MonitorFromWindow(owner, MONITOR_DEFAULTTONEAREST));
        if (modal.enabled)
            EnableWindow(owner, FALSE);
        reset_clock();
        if (!SetTimer(created, 1, 8, nullptr))
            throw std::runtime_error("Cannot start frontend clock");
        ShowWindow(created, SW_SHOW);
        update_activity();
        MSG message{};
        while (IsWindow(created)) {
            const auto result = GetMessageW(&message, nullptr, 0, 0);
            if (result == -1)
                throw std::runtime_error("Cannot read frontend input");
            if (!result) {
                PostQuitMessage(int(message.wParam));
                break;
            }
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
    }
};
FrontendWindow::FrontendWindow(Bitmap canvas, std::wstring class_name, std::wstring title) :
    impl_(std::make_unique<Impl>(*this, std::move(canvas), std::move(class_name), std::move(title))) {}
FrontendWindow::~FrontendWindow() = default;
void FrontendWindow::run(void* owner) {
    impl_->run(static_cast<HWND>(owner));
}
Bitmap& FrontendWindow::canvas() {
    return impl_->canvas;
}
void* FrontendWindow::window() const {
    return impl_->window;
}
void FrontendWindow::refresh() {
    if (impl_->window)
        InvalidateRect(impl_->window, nullptr, FALSE);
}
void FrontendWindow::close() {
    if (impl_->window)
        DestroyWindow(impl_->window);
}
void FrontendWindow::caption(std::wstring text) {
    impl_->title = std::move(text);
    if (impl_->window)
        SetWindowTextW(impl_->window, impl_->title.c_str());
}
void FrontendWindow::fullscreen(bool enabled) {
    if (impl_->fullscreen != enabled)
        impl_->toggle_fullscreen(1LL << 29);
}
bool FrontendWindow::fullscreen() const {
    return impl_->fullscreen;
}
void FrontendWindow::gamepad(bool enabled) {
    if (enabled && !impl_->gamepad)
        impl_->gamepad = std::make_unique<GamepadInput>();
    if (!enabled) {
        impl_->gamepad.reset();
        impl_->previous_pad = 0;
        impl_->pad_armed = false;
    }
    if (impl_->window) {
        if (enabled)
            SetPropW(impl_->window, L"TheWebGamepadMenus", reinterpret_cast<HANDLE>(1));
        else
            RemovePropW(impl_->window, L"TheWebGamepadMenus");
    }
}
void FrontendWindow::diagnostics(DiagnosticLog* reports) {
    impl_->reports = reports;
}
} // namespace web
