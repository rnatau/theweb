#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace web {
// MENU's linked-list selection, independently owned as stable vector indices.
// Layout, drawing and activation delays are separate from these controls.
class MenuSelection {
public:
    explicit MenuSelection(std::vector<std::int32_t> items);
    std::int32_t selected_id() const; // 0x402110, -4 for no selection.
    std::int32_t next();              // 0x402160, clamps at the final item.
    std::int32_t previous();          // 0x4021a8, clamps at the first item.
    void select(std::int32_t id);     // 0x4020d4, missing ID retains selection.
    void remember();                  // 0x402124.
    void restore();                   // 0x402130, re-finds the remembered ID.
    std::optional<std::size_t> selected_index() const {
        return selected_;
    }
    std::optional<std::size_t> remembered_index() const {
        return remembered_;
    }

private:
    std::vector<std::int32_t> items_;
    std::optional<std::size_t> selected_, remembered_;
};
} // namespace web
