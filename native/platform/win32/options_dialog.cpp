#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define UNICODE
#define _UNICODE
#include <windows.h>
#include "options_dialog.hpp"
#include "../../edition.hpp"
#include "camera_window.hpp"
#include "slideshow_window.hpp"
#include "keyboard_input.hpp"
#include <algorithm>
#include <cstring>
#include <string_view>

namespace web {
namespace {
constexpr int view_control = 100, sound_control = 101, volume_control = 102, status_control = 103, restart_control = 104, camera_control = 105, mode_control = 106, detail_control = 107,
              music_control = 108, music_volume_control = 109, first_key_control = 110, rate_control = 121, nudge_control = 122, pcm_control = 123, stereo_control = 124, defaults_control = 125,
              fullscreen_control = 126, scaling_control = 127;
constexpr UINT capture_key = WM_APP + 1;
thread_local HWND capture_window = nullptr;
constexpr std::array<const wchar_t*, 5> control_labels = {L"Left flipper", L"Right flipper", L"Nudge left", L"Nudge right", L"Nudge up"};
constexpr std::array<unsigned, 6> views = {2, 1, 0, 6, 5, 4};
constexpr std::array<unsigned, 4> sound_rates = {6000, 11025, 22050, 44100};
struct DialogState {
    GameOptions options;
    bool forced_off;
    std::filesystem::path assets;
    WindowDisplay display;
    const Edition& edition;
    int capturing = -1;
};
std::wstring key_name(unsigned key) {
    const auto scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
    wchar_t name[80]{};
    if (GetKeyNameTextW(LONG(scan << 16), name, int(std::size(name))))
        return name;
    return L"Key " + std::to_wstring(key);
}
HWND add_control(HWND dialog, const wchar_t* kind, const wchar_t* text, int id, DWORD style, int x, int y, int width, int height) {
    RECT box{x, y, x + width, y + height};
    MapDialogRect(dialog, &box);
    const auto control = CreateWindowExW(
        0,
        kind,
        text,
        WS_CHILD | WS_VISIBLE | style,
        box.left,
        box.top,
        box.right - box.left,
        box.bottom - box.top,
        dialog,
        reinterpret_cast<HMENU>(INT_PTR(id)),
        GetModuleHandleW(nullptr),
        nullptr);
    if (!control)
        throw std::runtime_error("Cannot create options control");
    SendMessageW(control, WM_SETFONT, SendMessageW(dialog, WM_GETFONT, 0, 0), TRUE);
    return control;
}
void refresh_keys(HWND dialog, const DialogState& state) {
    for (unsigned i = 0; i < control_labels.size(); ++i)
        SetDlgItemTextW(dialog, first_key_control + i, key_name(state.options.key(i)).c_str());
}
INT_PTR CALLBACK dialog_proc(HWND dialog, UINT message, WPARAM w, LPARAM l) {
    auto* state = reinterpret_cast<DialogState*>(GetWindowLongPtrW(dialog, DWLP_USER));
    try {
        if (message == WM_INITDIALOG) {
            state = reinterpret_cast<DialogState*>(l);
            SetWindowLongPtrW(dialog, DWLP_USER, l);
            capture_window = dialog;
            SetWindowTextW(dialog, L"The Web - Game options");
            auto add = [&](const wchar_t* kind, const wchar_t* label, int id, DWORD style, int x, int y, int width, int height) {
                const auto row = y;
                if (row >= 81)
                    y += 24; // Preserve the original dialog's music row.
                if (!state->edition.selectable_quality) {
                    if (row >= 34)
                        y -= 24; // Screen mode.
                    if (row >= 105)
                        y -= 24; // Graphic detail.
                    if (row >= 264)
                        y -= 24; // Effects sample rate.
                    if (row >= 304)
                        y -= 20; // PCM format and stereo.
                }
                if (!state->edition.selectable_nudge && row >= 284)
                    y -= 20;
                return add_control(dialog, kind, label, id, style, x, y, width, height);
            };
            if (state->edition.selectable_quality) {
                add(L"STATIC", L"Screen mode", -1, 0, 12, 14, 88, 12);
                const auto mode = add(L"COMBOBOX", L"", mode_control, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 108, 11, 130, 120);
                for (const auto label :
                     {L"640 x 480 - 256 colors", L"800 x 600 - 256 colors", L"1024 x 768 - 256 colors", L"640 x 480 - 32K colors", L"800 x 600 - 32K colors", L"1024 x 768 - 32K colors"})
                    SendMessageW(mode, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
                SendMessageW(mode, CB_SETCURSEL, state->options.screen_mode() - 1, 0);
            }
            add(L"STATIC", L"Table view", -1, 0, 12, 38, 88, 12);
            const auto view = add(L"COMBOBOX", L"", view_control, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 108, 35, 78, 100);
            for (unsigned i = 0; i < views.size(); ++i) {
                const auto name = L"View " + std::to_wstring(i + 1);
                SendMessageW(view, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(name.c_str()));
                if (views[i] == state->options.view())
                    SendMessageW(view, CB_SETCURSEL, i, 0);
            }
            add(L"BUTTON", L"Browse...", camera_control, WS_TABSTOP | BS_PUSHBUTTON, 192, 34, 46, 18);
            const auto sound = add(L"BUTTON", L"Sound effects and voices", sound_control, WS_TABSTOP | BS_AUTOCHECKBOX, 12, 60, 130, 14);
            SendMessageW(sound, BM_SETCHECK, state->options.sound_enabled() ? BST_CHECKED : BST_UNCHECKED, 0);
            add(L"STATIC", L"Volume", -1, 0, 145, 60, 40, 12);
            const auto volume = add(L"EDIT", std::to_wstring(state->options.volume()).c_str(), volume_control, WS_TABSTOP | WS_BORDER | ES_NUMBER | ES_AUTOHSCROLL, 188, 57, 50, 15);
            SendMessageW(volume, EM_SETLIMITTEXT, 3, 0);
            const auto music = add(L"BUTTON", L"Music", music_control, WS_TABSTOP | BS_AUTOCHECKBOX, 12, 80, 120, 14);
            SendMessageW(music, BM_SETCHECK, state->options.music_enabled() ? BST_CHECKED : BST_UNCHECKED, 0);
            add(L"STATIC", L"Volume", -1, 0, 145, 80, 40, 12);
            const auto music_volume =
                add(L"EDIT", std::to_wstring(state->options.music_volume() ? state->options.music_volume() : 63).c_str(), music_volume_control, WS_TABSTOP | WS_BORDER | ES_NUMBER, 188, 78, 50, 15);
            SendMessageW(music_volume, EM_SETLIMITTEXT, 3, 0);
            if (state->edition.selectable_quality) {
                add(L"STATIC", L"Graphic detail", -1, 0, 12, 84, 88, 12);
                const auto detail = add(L"COMBOBOX", L"", detail_control, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 108, 81, 130, 100);
                for (const auto label : {L"Low", L"Normal", L"High"})
                    SendMessageW(detail, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
                SendMessageW(detail, CB_SETCURSEL, state->options.graphic_detail() - 1, 0);
            }
            add(L"STATIC", L"Sound transitions", -1, 0, 12, 108, 88, 12);
            const auto restart = add(L"COMBOBOX", L"", restart_control, WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL, 108, 105, 130, 100);
            for (const auto label : {L"Instant", L"Quiet ball", L"No table balls"})
                SendMessageW(restart, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label));
            SendMessageW(restart, CB_SETCURSEL, state->options.music_restart_policy(), 0);
            add(L"STATIC", L"Select a control to change its key.", -1, 0, 12, 124, 130, 12);
            add(L"BUTTON", L"Default keys", defaults_control, WS_TABSTOP | BS_PUSHBUTTON, 150, 120, 88, 18);
            for (unsigned i = 0; i < control_labels.size(); ++i) {
                add(L"STATIC", control_labels[i], -1, 0, 12, 145 + 19 * i, 126, 12);
                add(L"BUTTON", L"", first_key_control + i, WS_TABSTOP | BS_PUSHBUTTON, 150, 140 + 19 * i, 88, 18);
            }
            refresh_keys(dialog, *state);
            if (state->edition.selectable_quality) {
                add(L"STATIC", L"Effects sample rate", -1, 0, 12, 244, 100, 12);
                const auto rate = add(L"COMBOBOX", L"", rate_control, WS_TABSTOP | CBS_DROPDOWNLIST, 120, 241, 118, 100);
                for (unsigned i = 0; i < sound_rates.size(); ++i) {
                    const auto label = std::to_wstring(sound_rates[i]) + L" Hz";
                    SendMessageW(rate, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                    if (sound_rates[i] == (state->options.sound_rate() ? state->options.sound_rate() : 22050))
                        SendMessageW(rate, CB_SETCURSEL, i, 0);
                }
            }
            if (state->edition.selectable_nudge) {
                const auto nudge = add(L"BUTTON", L"Visual nudge feedback", nudge_control, WS_TABSTOP | BS_AUTOCHECKBOX, 12, 264, 226, 14);
                SendMessageW(nudge, BM_SETCHECK, state->options.visual_nudge() ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            if (state->edition.selectable_quality) {
                const auto pcm = add(L"BUTTON", L"16-bit effects output", pcm_control, WS_TABSTOP | BS_AUTOCHECKBOX, 12, 284, 110, 14);
                SendMessageW(pcm, BM_SETCHECK, state->options.sound_16bit() ? BST_CHECKED : BST_UNCHECKED, 0);
                const auto stereo = add(L"BUTTON", L"Stereo", stereo_control, WS_TABSTOP | BS_AUTOCHECKBOX, 132, 284, 106, 14);
                SendMessageW(stereo, BM_SETCHECK, state->options.sound_stereo() ? BST_CHECKED : BST_UNCHECKED, 0);
            }
            add(L"STATIC",
                state->forced_off ? L"Sound is disabled by --mute for this run." : L"S/F1 start, Enter launch, C continue, P pause.\nF/J: flippers; arrows: volume.",
                status_control,
                0,
                12,
                304,
                226,
                20);
            add(L"BUTTON", L"Slideshow...", 120, WS_TABSTOP | BS_PUSHBUTTON, 12, 349, 66, 20);
            add(L"BUTTON", L"Save options", IDOK, WS_TABSTOP | BS_DEFPUSHBUTTON, 84, 349, 74, 20);
            add(L"BUTTON", L"Cancel", IDCANCEL, WS_TABSTOP | BS_PUSHBUTTON, 164, 349, 74, 20);
            const auto fullscreen = add(L"BUTTON", L"Fullscreen (this session)", fullscreen_control, WS_TABSTOP | BS_AUTOCHECKBOX, 12, 328, 96, 14);
            SendMessageW(fullscreen, BM_SETCHECK, state->display.fullscreen ? BST_CHECKED : BST_UNCHECKED, 0);
            add(L"STATIC", L"Crisp DMD", scaling_control, 0, 120, 328, 118, 14);
            RECT parent{}, box{};
            GetWindowRect(GetParent(dialog), &parent);
            GetWindowRect(dialog, &box);
            const int width = box.right - box.left, height = box.bottom - box.top;
            MONITORINFO monitor{};
            monitor.cbSize = sizeof(monitor);
            if (GetMonitorInfoW(MonitorFromWindow(dialog, MONITOR_DEFAULTTONEAREST), &monitor)) {
                const auto& work = monitor.rcWork;
                const int x = std::clamp((parent.left + parent.right - width) / 2, work.left, std::max(work.left, work.right - width));
                const int y = std::clamp((parent.top + parent.bottom - height) / 2, work.top, std::max(work.top, work.bottom - height));
                SetWindowPos(dialog, nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            }
            return TRUE;
        }
        if (!state)
            return FALSE;
        if (message == capture_key && state->capturing >= 0) {
            if (w == VK_ESCAPE) {
                state->capturing = -1;
                refresh_keys(dialog, *state);
                SetDlgItemTextW(dialog, status_control, L"Key change cancelled.");
                return TRUE;
            }
            try {
                state->options.key(unsigned(state->capturing), unsigned(w));
                SetFocus(GetDlgItem(dialog, first_key_control + state->capturing));
                state->capturing = -1;
                refresh_keys(dialog, *state);
                SetDlgItemTextW(dialog, status_control, L"Key changed. Save options to apply.");
            } catch (const std::exception& error) {
                SetDlgItemTextA(dialog, status_control, error.what());
            }
            return TRUE;
        }
        if (message == WM_COMMAND) {
            const auto id = LOWORD(w);
            if (id == camera_control && HIWORD(w) == BN_CLICKED) {
                const auto selected = SendDlgItemMessageW(dialog, view_control, CB_GETCURSEL, 0, 0);
                if (selected < 0 || std::size_t(selected) >= views.size())
                    return TRUE;
                auto draft = state->options;
                draft.view(views[std::size_t(selected)]);
                draft.sound_enabled(IsDlgButtonChecked(dialog, sound_control) == BST_CHECKED);
                state->edition.normalize_options(draft);
                BOOL valid = FALSE;
                const auto volume = GetDlgItemInt(dialog, volume_control, &valid, FALSE);
                if (valid && volume <= 127)
                    draft.volume(volume);
                if (const auto view = choose_camera(dialog, state->assets, draft, state->forced_off)) {
                    const auto index = std::find(views.begin(), views.end(), *view) - views.begin();
                    SendDlgItemMessageW(dialog, view_control, CB_SETCURSEL, index, 0);
                    SetDlgItemTextW(dialog, status_control, L"View selected. Save options to apply.");
                }
                return TRUE;
            }
            if (id == 120 && HIWORD(w) == BN_CLICKED) {
                show_slideshow(dialog, state->assets, state->options);
                return TRUE;
            }
            if (id == defaults_control && HIWORD(w) == BN_CLICKED) {
                state->capturing = -1;
                state->options.reset_controls();
                refresh_keys(dialog, *state);
                SetDlgItemTextW(dialog, status_control, L"Default keys restored. Save options to apply.");
                return TRUE;
            }
            if (id >= first_key_control && id < first_key_control + control_labels.size() && HIWORD(w) == BN_CLICKED) {
                state->capturing = id - first_key_control;
                refresh_keys(dialog, *state);
                SetDlgItemTextW(dialog, id, L"Press a key...");
                SetDlgItemTextW(dialog, status_control, L"Press the new key. Escape cancels the key change.");
                return TRUE;
            }
            if (id == IDCANCEL) {
                EndDialog(dialog, IDCANCEL);
                return TRUE;
            }
            if (id == IDOK) {
                BOOL valid = FALSE;
                const auto volume = GetDlgItemInt(dialog, volume_control, &valid, FALSE);
                if (!valid || volume > 127) {
                    SetDlgItemTextW(dialog, status_control, L"Enter an effects volume between 0 and 127.");
                    SetFocus(GetDlgItem(dialog, volume_control));
                    return TRUE;
                }
                const auto music_volume = GetDlgItemInt(dialog, music_volume_control, &valid, FALSE);
                if (!valid || music_volume > 255 || (IsDlgButtonChecked(dialog, music_control) == BST_CHECKED && !music_volume)) {
                    SetDlgItemTextW(dialog, status_control, L"Enter a music volume between 1 and 255, or turn music off.");
                    SetFocus(GetDlgItem(dialog, music_volume_control));
                    return TRUE;
                }
                state->options.music_volume(IsDlgButtonChecked(dialog, music_control) == BST_CHECKED ? music_volume : 0);
                const auto view = SendDlgItemMessageW(dialog, view_control, CB_GETCURSEL, 0, 0);
                if (view < 0 || std::size_t(view) >= views.size())
                    return TRUE;
                const auto restart = SendDlgItemMessageW(dialog, restart_control, CB_GETCURSEL, 0, 0);
                if (restart < 0 || restart > 2)
                    return TRUE;
                state->options.view(views[std::size_t(view)]);
                if (state->edition.selectable_quality) {
                    const auto mode = SendDlgItemMessageW(dialog, mode_control, CB_GETCURSEL, 0, 0);
                    if (mode < 0 || mode > 5)
                        return TRUE;
                    const auto detail = SendDlgItemMessageW(dialog, detail_control, CB_GETCURSEL, 0, 0);
                    if (detail < 0 || detail > 2)
                        return TRUE;
                    state->options.screen_mode(unsigned(mode) + 1);
                    state->options.graphic_detail(unsigned(detail) + 1);
                }
                state->options.music_restart_policy(unsigned(restart));
                state->options.volume(volume);
                if (state->edition.selectable_quality) {
                    const auto rate = SendDlgItemMessageW(dialog, rate_control, CB_GETCURSEL, 0, 0);
                    if (rate < 0 || std::size_t(rate) >= sound_rates.size())
                        return TRUE;
                    state->options.sound_rate(IsDlgButtonChecked(dialog, sound_control) == BST_CHECKED ? sound_rates[std::size_t(rate)] : 0);
                    state->options.sound_16bit(IsDlgButtonChecked(dialog, pcm_control) == BST_CHECKED);
                    state->options.sound_stereo(IsDlgButtonChecked(dialog, stereo_control) == BST_CHECKED);
                } else {
                    state->options.sound_enabled(IsDlgButtonChecked(dialog, sound_control) == BST_CHECKED);
                }
                if (state->edition.selectable_nudge)
                    state->options.visual_nudge(IsDlgButtonChecked(dialog, nudge_control) == BST_CHECKED);
                state->edition.normalize_options(state->options);
                state->display.fullscreen = IsDlgButtonChecked(dialog, fullscreen_control) == BST_CHECKED;
                state->display.scaling = PresentationScaling::fit;
                EndDialog(dialog, IDOK);
                return TRUE;
            }
        }
    } catch (const std::exception& error) {
        MessageBoxA(dialog, error.what(), "Game options", MB_ICONERROR);
        EndDialog(dialog, IDCANCEL);
        return TRUE;
    }
    return FALSE;
}
LRESULT CALLBACK capture_filter(int code, WPARAM w, LPARAM l) {
    // DialogBox owns the loop. Capture before IsDialogMessage consumes Tab,
    // arrows or Enter, so reserved keys can produce a useful explanation.
    if (code >= 0 && w == PM_REMOVE) {
        auto& message = *reinterpret_cast<MSG*>(l);
        const auto dialog = GetAncestor(message.hwnd, GA_ROOT);
        const auto* state = dialog == capture_window ? reinterpret_cast<DialogState*>(GetWindowLongPtrW(dialog, DWLP_USER)) : nullptr;
        if (state && state->capturing >= 0 && (message.message == WM_KEYDOWN || message.message == WM_SYSKEYDOWN)) {
            SendMessageW(dialog, capture_key, windows::distinct_key(unsigned(message.wParam), message.lParam), message.lParam);
            message.message = WM_NULL;
        }
    }
    return CallNextHookEx(nullptr, code, w, l);
}
} // namespace
bool edit_options(void* owner, GameOptions& options, bool forced_off, const std::filesystem::path& assets, WindowDisplay& display, const Edition& edition) {
    // DLGTEMPLATE is packed on two-byte boundaries. Empty menu/class/title,
    // then DS_SETFONT's point size and a null-terminated font name.
    const DLGTEMPLATE
        header{WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME | DS_SETFONT, 0, 0, 0, 0, 250, short(405 - (edition.selectable_quality ? 0 : 92) - (edition.selectable_nudge ? 0 : 20))};
    std::vector<WORD> words(sizeof(header) / sizeof(WORD));
    std::memcpy(words.data(), &header, sizeof(header));
    words.insert(words.end(), {0, 0, 0, 9});
    for (const wchar_t letter : std::wstring_view(L"Segoe UI"))
        words.push_back(WORD(letter));
    words.push_back(0);
    DialogState state{options, forced_off, assets, display, edition};
    edition.normalize_options(state.options);
    const auto hook = SetWindowsHookExW(WH_GETMESSAGE, capture_filter, nullptr, GetCurrentThreadId());
    if (!hook)
        throw std::runtime_error("Cannot install options key capture");
    const auto result = DialogBoxIndirectParamW(GetModuleHandleW(nullptr), reinterpret_cast<const DLGTEMPLATE*>(words.data()), static_cast<HWND>(owner), dialog_proc, reinterpret_cast<LPARAM>(&state));
    UnhookWindowsHookEx(hook);
    capture_window = nullptr;
    if (result == -1)
        throw std::runtime_error("Cannot open game options");
    if (result != IDOK)
        return false;
    options = state.options;
    display = state.display;
    return true;
}
} // namespace web
