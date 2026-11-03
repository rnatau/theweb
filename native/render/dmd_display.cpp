#include "dmd_display.hpp"
#include "../rules/missions.hpp"

namespace web {
namespace {
constexpr int overlay_priority = -1, exclusive_priority = 1;
constexpr unsigned power_page_frames = 18, arena_page_frames = 23, lock_page_frames = 18;
constexpr unsigned unlit_hole_page_frames = 24;
constexpr unsigned power_animation = 14, sphere_animation = 125;
constexpr unsigned first_station_animation = 15, last_station_animation = 29;
} // namespace
DmdDisplay::DmdDisplay(const std::filesystem::path& assets, bool fit_hideout_caption) :
    renderer_(assets, fit_hideout_caption),
    video_renderer_(renderer_, assets),
    animations_(assets) {
    reset();
}
void DmdDisplay::reset() {
    animations_.stop();
    accumulated_ = score_counter_ = grid_counter_ = 0;
    prepared_ = false;
    ball_character_ = 'X';
    page_ = {};
    bonus_values_ = {};
    mission_values_ = {};
    scoring_values_ = {};
    video_total_ = 0;
    video_perfect_ = false;
    lock_values_ = {};
    ultimate_values_ = {};
    serve_values_ = {};
    entry_values_ = {};
    record_values_ = {};
    match_values_ = {};
    status_values_ = {};
    status_selection_ = 0;
    effects_volume_ = 127;
    attract_ = {};
    attract_values_ = {};
    DotMatrixRenderer::ScorePage page;
    page.balls.fill(1);
    frame_ = renderer_.score_page(page);
    image_ = DotMatrixRenderer::leds(frame_);
}
void DmdDisplay::station_finish_animations() {
    // Both editions deliberately clear the ended frenzy's artwork, including
    // a backdrop queued behind another animation, without cancelling other awards.
    animations_.discard(first_station_animation, last_station_animation);
}
bool DmdDisplay::accept_page_request(int priority, bool replace) {
    if (!replace && page_.remaining)
        return false;
    if (priority == exclusive_priority)
        animations_.stop();
    return true;
}
void DmdDisplay::mission_page(rules::MissionPage page, unsigned ticks, int priority, bool replace) {
    // 0x41c11c conditionally accepts; 0x41c15c always replaces. Only an
    // accepted exclusive request clears the animation queue.
    if (!accept_page_request(priority, replace))
        return;
    mission_page_ = page;
    page_ = {Page::mission, ticks, priority};
}
void DmdDisplay::effects_volume(unsigned value) {
    if (value > 127)
        throw std::out_of_range("Effects volume must be between 0 and 127");
    effects_volume_ = value;
    animations_.stop();
    page_ = {Page::effects_volume, 24, 1};
}
void DmdDisplay::status_page(int selection) {
    animations_.stop();
    status_selection_ = selection;
    page_ = {Page::status, 2, 1};
}
void DmdDisplay::warning(int value, unsigned duration) {
    animations_.stop();
    warning_ = value;
    page_ = {Page::warning, duration, exclusive_priority};
}
void DmdDisplay::scoring_page(rules::ScoringPage page, unsigned ticks, int priority, bool replace) {
    if (!accept_page_request(priority, replace))
        return;
    scoring_page_ = page;
    page_ = {Page::scoring, ticks, priority};
}
void DmdDisplay::award_collect(rules::ScoringPage page, unsigned ticks, int priority, std::uint32_t units) {
    // Atomic original 0x41c198: rejected requests retain the old shared value.
    if (page_.remaining && priority != exclusive_priority)
        return;
    scoring_amount({units % rules::ScoreValue::radix, (units / rules::ScoreValue::radix) % 10});
    scoring_page(page, ticks, priority, true);
}
void DmdDisplay::video_page(rules::VideoPage page, unsigned ticks, int priority) {
    accept_page_request(priority);
    video_page_ = page;
    page_ = {Page::video, ticks, priority};
}
void DmdDisplay::lock_page(rules::LockPage page, unsigned ticks, int priority, bool replace) {
    if (!accept_page_request(priority, replace))
        return;
    lock_page_ = page;
    page_ = {Page::lock_award, ticks, priority};
}
void DmdDisplay::explosion_page(rules::ExplosionPage page, unsigned ticks, int priority) {
    accept_page_request(priority);
    explosion_page_ = page;
    page_ = {Page::explosion, ticks, priority};
}
void DmdDisplay::ultimate_page(rules::UltimatePage page, unsigned ticks, int priority) {
    if (page == rules::UltimatePage::victory_award) {
        scoring_page(rules::ScoringPage::flashing_award, ticks, priority, true);
        return;
    }
    accept_page_request(priority);
    ultimate_page_ = page;
    page_ = {Page::ultimate, ticks, priority};
}
void DmdDisplay::serve_page(rules::ServePage page, unsigned ticks, int priority) {
    accept_page_request(priority);
    serve_page_ = page;
    page_ = {Page::serve, ticks, priority};
}
void DmdDisplay::record_page(rules::RecordPage page, unsigned ticks, int priority) {
    accept_page_request(priority);
    record_page_ = page;
    page_ = {page == rules::RecordPage::initials ? Page::entry : Page::record_result, ticks, priority};
}
void DmdDisplay::match_page(unsigned ticks, int priority) {
    // Original priority 11 allows the underlying animation queue to survive.
    accept_page_request(priority);
    page_ = {Page::match, ticks, priority};
}
void DmdDisplay::end_stop_animations() {
    stop_animations();
    attract_.state.page = 0;
    attract_.state.timer = 2;
}
void DmdDisplay::power_level(int level, bool animation_allowed) {
    if (animation_allowed)
        animation(power_animation, DmdAnimationPlayer::replace);
    if (level != 7) {
        power_level_ = level;
        page_ = {Page::power, power_page_frames, overlay_priority};
    }
    if (level == 8)
        animation(sphere_animation);
}
void DmdDisplay::arena_status(int ramps, int lock_at, int extra_ball_at) {
    arena_ = {ramps, lock_at, extra_ball_at};
    page_ = {Page::arena, arena_page_frames, overlay_priority};
}
void DmdDisplay::lock_ready() {
    page_ = {Page::lock, lock_page_frames, overlay_priority};
}
void DmdDisplay::unlit_hole() {
    // 0x41c11c accepts this request only when no text page is active.
    if (!page_.remaining)
        page_ = {Page::unlit_hole, unlit_hole_page_frames, overlay_priority};
}
void DmdDisplay::loop_combo_page(rules::LoopComboPage page, int count, rules::ScoreValue amount) {
    loop_combo_page_ = page;
    loop_combo_count_ = count;
    loop_combo_amount_ = amount;
    constexpr unsigned loop_frames = 12, mania_frames = 24, mania_score_frames = 9;
    const auto duration = page == rules::LoopComboPage::loops ? loop_frames : page == rules::LoopComboPage::mania_score ? mania_score_frames : mania_frames;
    page_ = {Page::loop_combo, duration, overlay_priority};
}
DotMatrixFrame DmdDisplay::render_page(const DotMatrixFrame& backdrop) {
    const auto counter = score_counter_++;
    switch (page_.kind) {
        case Page::bonus:
            return renderer_.bonus_page(bonus_values_, counter, &backdrop);
        case Page::warning:
            return renderer_.warning_page(warning_, counter, &backdrop);
        case Page::power:
            return renderer_.power_page(power_level_, counter, &backdrop);
        case Page::arena:
            return renderer_.arena_page(arena_, counter, &backdrop);
        case Page::lock:
            return renderer_.lock_page(counter, &backdrop);
        case Page::unlit_hole:
            return renderer_.unlit_hole_page(counter, &backdrop);
        case Page::loop_combo:
            // Original selectors 24/25 (0x41fb8d/0x41fbcb) leave the
            // animation visible for the first twelve ticks of the request.
            if ((loop_combo_page_ == rules::LoopComboPage::mania_remaining || loop_combo_page_ == rules::LoopComboPage::mania_ready) && page_.remaining >= 12)
                return backdrop;
            return renderer_.loop_combo_page(loop_combo_page_, loop_combo_count_, loop_combo_amount_, counter, &backdrop);
        case Page::mission:
            return renderer_.mission_page(mission_page_, mission_values_, page_.remaining, counter, &backdrop);
        case Page::scoring:
            return renderer_.scoring_page(scoring_page_, scoring_values_, page_.remaining, counter, &backdrop);
        case Page::video:
            return video_renderer_.page(video_page_, video_total_, video_perfect_, page_.remaining, counter, backdrop);
        case Page::lock_award:
            return renderer_.lock_award_page(lock_page_, lock_values_, counter, &backdrop);
        case Page::explosion:
            return renderer_.explosion_page(explosion_page_, counter, &backdrop);
        case Page::ultimate:
            return renderer_.ultimate_page(ultimate_page_, ultimate_values_, counter, &backdrop);
        case Page::serve:
            return renderer_.serve_page(serve_page_, serve_values_, page_.remaining, counter, &backdrop);
        case Page::entry:
            return renderer_.record_entry_page(entry_values_, counter, &backdrop);
        case Page::record_result:
            return renderer_.record_result_page(record_page_, record_values_, page_.remaining, counter, &backdrop);
        case Page::match:
            return renderer_.match_page(match_values_, page_.remaining, counter, &backdrop);
        case Page::status:
            return renderer_.status_page(status_selection_, status_values_, counter, &backdrop);
        case Page::effects_volume:
            return renderer_.effects_volume_page(effects_volume_, counter, &backdrop);
        default:
            return backdrop;
    }
}
void DmdDisplay::advance(std::int32_t elapsed, DotMatrixRenderer::ScorePage page, const std::optional<DotMatrixRenderer::BonusPage>& bonus, const std::optional<VideoScene>& video, bool startup) {
    constexpr std::uint32_t frame_interval = 0xaaa; // 0x420988: independent DMD frame threshold.
    if (elapsed < 0)
        throw std::invalid_argument("Negative DMD elapsed time");
    accumulated_ += std::uint32_t(elapsed);
    if (bonus)
        bonus_values_ = *bonus;
    if (attract_values_.edges & input_action::both_flippers)
        attract_.state.skip = 1;
    attract_values_.edges = 0;
    if (!prepared_) {
        const bool exclusive_page = page_.remaining && page_.priority > 0;
        if (!exclusive_page && !video && !startup)
            animations_.step();
        const bool animation_frame = animations_.active() && !exclusive_page && !video;
        const auto backdrop = startup ? DotMatrixFrame{} : animation_frame ? animations_.frame() : renderer_.grid_frame(grid_counter_++);
        if (startup)
            frame_ = renderer_.startup_page(startup_version_);
        else if (video)
            frame_ = video->draw ? video_renderer_.scene(video->state, video->score, backdrop) : backdrop;
        else if (page_.remaining && (page_.priority != 0 || !animation_frame))
            frame_ = render_page(backdrop);
        else if (animation_frame)
            frame_ = backdrop;
        else if (!page.started) {
            frame_ = attract_.render(renderer_, attract_values_, backdrop);
            ++score_counter_;
        } else {
            page.counter = score_counter_++;
            // 0x41d009 updates the shared caption only for turns below four,
            // and only on frames that draw it. The end-game gap retains the
            // last displayed ball instead of briefly inventing a fourth ball.
            const bool ball_visible = ((page.counter + 1) & 0x30) != 0 || page.players != 4;
            if (ball_visible && page.balls.at(page.player) < 4)
                ball_character_ = char('0' + page.balls.at(page.player));
            page.ball_character = ball_character_;
            frame_ = renderer_.score_page(page, &backdrop);
        }
        image_ = DotMatrixRenderer::leds(frame_);
        attract_.state.skip = 0; // Original 0x420972, after any prepared text/animation frame.
        prepared_ = true;
    }
    if (accumulated_ > frame_interval) {
        accumulated_ -= frame_interval;
        prepared_ = false;
        if (page_.remaining && (page_.priority != 0 || !animations_.active()))
            if (!--page_.remaining)
                page_ = {};
    }
}
} // namespace web
