#include "frontend_menus.hpp"

namespace web {
namespace menu_item {
constexpr int play = 101, slideshow = 102, options = 103, quit = 104;
constexpr int first_options_page = 201, last_options_page = 205, back = 206;
constexpr int screen_mode_base = 300, first_screen_mode = 301, last_screen_mode = 306;
constexpr int flip_on = 351, flip_off = 352;
constexpr int camera_base = 400, first_camera = 401, last_camera = 406;
constexpr int first_music_policy = 501, last_music_policy = 503;
constexpr int detail_base = 600, first_detail = 601, last_detail = 603;
} // namespace menu_item
FrontendMenus::FrontendMenus(
    const DotMatrixRenderer& renderer,
    const FrontendAssets& assets,
    GameOptions& options,
    std::span<const BackboxMenu::Item> option_items,
    std::span<const BackboxMenu::Item> main_items) :
    options_(options),
    camera_(assets) {
    for (unsigned i = 0; i < lists_.size(); ++i)
        lists_[i] = std::make_unique<BackboxMenu>(renderer, BackboxMenu::List(i));
    if (!option_items.empty())
        lists_[Page::options] = std::make_unique<BackboxMenu>(renderer, option_items);
    if (!main_items.empty())
        lists_[Page::main] = std::make_unique<BackboxMenu>(renderer, main_items);
    lists_[Page::screen_mode]->select(menu_item::screen_mode_base + options.screen_mode());
    lists_[Page::screen_flip]->select(options.screen_flip() ? menu_item::flip_on : menu_item::flip_off);
    lists_[Page::music_restart]->select(menu_item::first_music_policy + options.music_restart_policy());
    lists_[Page::graphic_detail]->select(menu_item::detail_base + options.graphic_detail());
    camera_.select(menu_item::camera_base + options.camera_selection());
}
bool FrontendMenus::input(std::uint32_t edges, SoundSink* sounds) {
    return page_ == Page::camera ? camera_.input(edges, sounds) : lists_[page_]->input(edges, sounds);
}
bool FrontendMenus::activation_complete() const {
    return activation_remaining() < 1;
}
void FrontendMenus::remember() {
    if (page_ == Page::camera)
        camera_.remember();
    else
        lists_[page_]->remember();
}
void FrontendMenus::restore() {
    if (page_ == Page::camera)
        camera_.restore();
    else
        lists_[page_]->restore();
}
FrontendMenus::Action FrontendMenus::accept() {
    const auto id = selected_id();
    if (id == menu_item::play)
        return Action::play;
    if (id == menu_item::slideshow)
        return Action::slideshow;
    if (id == menu_item::quit)
        return Action::quit;
    if (id == menu_item::options) {
        page_ = Page::options;
        lists_[Page::options]->select(menu_item::first_options_page);
    } else if (id >= menu_item::first_options_page && id <= menu_item::last_options_page) {
        constexpr std::array<Page, 5> pages = {Page::screen_mode, Page::screen_flip, Page::graphic_detail, Page::camera, Page::music_restart};
        page_ = pages[unsigned(id - menu_item::first_options_page)];
        remember();
    } else if (id == menu_item::back) {
        page_ = Page::main;
        lists_[Page::main]->select(menu_item::play);
    } else {
        if (id >= menu_item::first_screen_mode && id <= menu_item::last_screen_mode)
            options_.screen_mode(unsigned(id - menu_item::screen_mode_base));
        else if (id == menu_item::flip_on || id == menu_item::flip_off)
            options_.screen_flip(id == menu_item::flip_on);
        else if (id >= menu_item::first_camera && id <= menu_item::last_camera)
            options_.camera_selection(unsigned(id - menu_item::camera_base));
        else if (id >= menu_item::first_music_policy && id <= menu_item::last_music_policy)
            options_.music_restart_policy(unsigned(id - menu_item::first_music_policy));
        else if (id >= menu_item::first_detail && id <= menu_item::last_detail)
            options_.graphic_detail(unsigned(id - menu_item::detail_base));
        page_ = Page::options;
    }
    return Action::none;
}
FrontendMenus::Action FrontendMenus::escape() {
    restore();
    if (page_ == Page::main)
        return Action::quit;
    if (page_ == Page::options) {
        page_ = Page::main;
        lists_[Page::main]->select(menu_item::play);
    } else
        page_ = Page::options;
    return Action::none;
}
DotMatrixFrame FrontendMenus::step(const DotMatrixFrame& backdrop, std::int32_t elapsed) {
    return page_ == Page::camera ? camera_.step(backdrop, elapsed) : lists_[page_]->step(backdrop, elapsed, page_ == Page::screen_flip ? 9 : 1);
}
std::int32_t FrontendMenus::selected_id() const {
    return page_ == Page::camera ? camera_.selection().selected_id() : lists_[page_]->selection().selected_id();
}
unsigned FrontendMenus::first_visible() const {
    return page_ == Page::camera ? camera_.first_visible() : lists_[page_]->first_visible();
}
std::uint32_t FrontendMenus::counter() const {
    return page_ == Page::camera ? camera_.counter() : lists_[page_]->counter();
}
std::int32_t FrontendMenus::activation_remaining() const {
    return page_ == Page::camera ? camera_.activation_remaining() : lists_[page_]->activation_remaining();
}
} // namespace web
