#include "menu_selection.hpp"
#include <algorithm>

namespace web {
MenuSelection::MenuSelection(std::vector<std::int32_t> items) :
    items_(std::move(items)) {
    if (!items_.empty())
        selected_ = 0;
}
std::int32_t MenuSelection::selected_id() const {
    return selected_ ? items_[*selected_] : -4;
}
std::int32_t MenuSelection::next() {
    if (selected_ && *selected_ + 1 < items_.size())
        ++*selected_;
    return selected_id();
}
std::int32_t MenuSelection::previous() {
    if (selected_ && *selected_)
        --*selected_;
    return selected_id();
}
void MenuSelection::select(std::int32_t id) {
    if (const auto item = std::find(items_.begin(), items_.end(), id); item != items_.end())
        selected_ = std::size_t(item - items_.begin());
}
void MenuSelection::remember() {
    remembered_ = selected_;
}
void MenuSelection::restore() {
    if (remembered_)
        select(items_[*remembered_]);
    remembered_.reset();
}
} // namespace web
