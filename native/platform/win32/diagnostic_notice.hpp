#pragma once
#include <windows.h>
#include <string>
#include <algorithm>

namespace web {
inline void draw_diagnostic_notice(HDC dc, RECT client, const std::string& notice) {
    if (notice.empty())
        return;
    const auto saved = SaveDC(dc);
    RECT area{8, std::max(8L, client.bottom - 40), std::max(8L, client.right - 8), client.bottom - 8};
    SetBkColor(dc, RGB(32, 20, 0));
    SetTextColor(dc, RGB(255, 220, 120));
    ExtTextOutW(dc, 0, 0, ETO_OPAQUE, &area, nullptr, 0, nullptr);
    const std::wstring text(notice.begin(), notice.end()); // Host notice labels are ASCII.
    SelectObject(dc, GetStockObject(DEFAULT_GUI_FONT));
    DrawTextW(dc, text.c_str(), int(text.size()), &area, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    RestoreDC(dc, saved);
}
} // namespace web
