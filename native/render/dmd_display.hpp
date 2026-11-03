#pragma once
#include "dmd_animation.hpp"
#include "video_renderer.hpp"
#include "attract_display.hpp"
#include "../rules/missions.hpp"
#include "../rules/power_levels.hpp"
#include "../rules/ramp_awards.hpp"
#include "../rules/single_ball_awards.hpp"
#include "../rules/space_station.hpp"
#include "../rules/game_end.hpp"
#include "../rules/status.hpp"
#include <optional>

namespace web {
// Presentation-only owner. Recovered rules remain the authority for scores,
// while original DMD requests and its independent update cadence live here.
class DmdDisplay : public rules::PowerLevelEffects,
                   public rules::RampAwardEffects,
                   public rules::LoopComboEffects,
                   public rules::MissionEffects,
                   public rules::ScoringEffects,
                   public rules::SingleBallAwardEffects,
                   public rules::VideoModeEffects,
                   public rules::LockEffects,
                   public rules::ExplosionEffects,
                   public rules::SpaceStationEffects,
                   public rules::UltimateEffects,
                   public rules::ServeEffects,
                   public rules::RecordEffects,
                   public rules::GameEndEffects,
                   public rules::StatusEffects {
public:
    explicit DmdDisplay(const std::filesystem::path& assets, bool fit_hideout_caption = false);
    void startup_version(std::string_view version) {
        startup_version_ = version;
    }
    void reset();
    void initialize_startup() {
        reset();
        frame_ = {};
        image_ = DotMatrixRenderer::leds(frame_);
    }
    void prepare_exit(const rules::ScoreRecord& champion) {
        frame_ = renderer_.exit_page(champion);
        image_ = DotMatrixRenderer::leds(frame_);
    }
    bool pending_frame() const {
        return !prepared_;
    }
    void animation(unsigned id, std::uint32_t flags = 0) {
        animations_.request(id, flags);
    }
    void stop_animations() {
        animations_.stop();
    }
    void animation_sounds(SoundSink* sink) {
        animations_.sound_step(sink);
    }
    void warning(int value, unsigned duration);
    void status_page(int selection) override;
    void effects_volume(unsigned value);
    void status_context(DotMatrixRenderer::StatusValues values) {
        status_values_ = values;
    }
    void power_level(int level, bool animation_allowed) override;
    void ramp_animation(unsigned id, std::uint32_t flags) override {
        animation(id, flags);
    }
    void arena_status(int ramps, int lock_at, int extra_ball_at) override;
    void lock_ready() override;
    void unlit_hole();
    void loop_combo_animation(unsigned id, std::uint32_t flags) override {
        animation(id, flags);
    }
    void loop_combo_page(rules::LoopComboPage page, int count, rules::ScoreValue amount) override;
    void clear_page() {
        page_ = {};
    }
    void show_bonus() {
        // Phase four refreshes selector 57 for three DMD ticks, including
        // its final request on the frame that changes the rules phase.
        animations_.stop();
        page_ = {Page::bonus, 3, 1};
    }
    unsigned mission_animation_active() const override {
        return animations_.state().active;
    }
    void mission_animation(unsigned id, std::uint32_t flags, unsigned repeats = 1) override {
        animations_.request(id, flags, repeats);
    }
    void mission_stop_animation() override {
        animations_.stop();
    }
    void mission_clear_page() override {
        clear_page();
    }
    void mission_page(rules::MissionPage page, unsigned ticks, int priority, bool replace) override;
    void mission_progress(int percent, int seconds) override {
        mission_values_.percent = percent;
        mission_values_.seconds = seconds;
    }
    void mission_context(int total, int floor, int rounds) {
        mission_values_.total = total;
        mission_values_.floor = floor;
        mission_values_.rounds = rounds;
    }
    bool scoring_award_page_active() const override {
        return page_.kind == Page::scoring && scoring_page_ == rules::ScoringPage::award;
    }
    void scoring_animation(unsigned id, std::uint32_t flags) override {
        animation(id, flags);
    }
    void scoring_page(rules::ScoringPage page, unsigned ticks, int priority, bool replace) override;
    void scoring_amount(rules::ScoreValue amount) override {
        scoring_values_.amount = amount;
    }
    void scoring_skillshot(std::uint32_t value) override {
        scoring_values_.skillshot = value;
    }
    void scoring_context(std::uint32_t ramp_value, int superjet_total, bool super_skillshot) {
        scoring_values_.ramp_value = ramp_value;
        scoring_values_.superjet_total = superjet_total;
        scoring_values_.super_skillshot = super_skillshot;
    }
    void award_animation(unsigned id, std::uint32_t flags, unsigned repeats = 1) override {
        animations_.request(id, flags, repeats);
    }
    void award_value(rules::ScoreValue amount) override {
        scoring_amount(amount);
    }
    void award_page(rules::ScoringPage page, unsigned ticks, int priority) override {
        scoring_page(page, ticks, priority, false);
    }
    void award_collect(rules::ScoringPage page, unsigned ticks, int priority, std::uint32_t units) override;
    void video_page(rules::VideoPage page, unsigned ticks, int priority) override;
    void lock_page(rules::LockPage page, unsigned ticks, int priority, bool replace) override;
    void explosion_page(rules::ExplosionPage page, unsigned ticks, int priority) override;
    void ultimate_page(rules::UltimatePage page, unsigned ticks, int priority) override;
    void serve_page(rules::ServePage page, unsigned ticks, int priority) override;
    void record_page(rules::RecordPage page, unsigned ticks, int priority) override;
    bool record_page_active(rules::RecordPage page) const override {
        return page_.remaining && record_page_ == page && (page_.kind == Page::entry || page_.kind == Page::record_result);
    }
    void record_context(DotMatrixRenderer::RecordValues values) {
        record_values_ = values;
    }
    void match_page(unsigned ticks, int priority) override;
    void end_clear_page() override {
        clear_page();
    }
    void end_stop_animations() override;
    void attract_context(AttractValues values) {
        attract_values_ = std::move(values);
    }
    const AttractState& attract_state() const {
        return attract_.state;
    }
    void match_context(DotMatrixRenderer::MatchValues values) {
        match_values_ = values;
    }
    void record_entry_context(DotMatrixRenderer::EntryValues values) {
        entry_values_ = values;
    }
    void serve_context(DotMatrixRenderer::ServeValues values) {
        serve_values_ = values;
    }
    void ultimate_clear_page() override {
        clear_page();
    }
    void ultimate_stop_animations() override {
        stop_animations();
    }
    unsigned ultimate_animation_active() const override {
        return animations_.state().active;
    }
    void ultimate_context(rules::UltimateDisplayValues values) {
        ultimate_values_ = values;
    }
    unsigned station_animation_active() const override {
        return animations_.state().active;
    }
    void station_stop_animations() override {
        stop_animations();
    }
    void station_finish_animations() override;
    void lock_stop_animations() override {
        stop_animations();
    }
    void lock_context(int selection, int seconds, int remaining) {
        lock_values_ = {selection, seconds, remaining};
    }
    void video_stop_animations() override {
        stop_animations();
    }
    void video_clear_page() override {
        clear_page();
    }
    void video_animation(unsigned id, std::uint32_t flags) override {
        animation(id, flags);
    }
    void video_context(std::uint32_t total, bool perfect) {
        video_total_ = total;
        video_perfect_ = perfect;
    }
    unsigned remaining_ticks() const {
        return page_.remaining;
    }
    bool accepts_video_scene() const {
        return !prepared_ && !page_.remaining;
    }
    struct VideoScene {
        rules::VideoModeState state;
        rules::ScoreValue score;
        bool draw = true;
    };
    void
    advance(std::int32_t elapsed, DotMatrixRenderer::ScorePage page, const std::optional<DotMatrixRenderer::BonusPage>& bonus = {}, const std::optional<VideoScene>& video = {}, bool startup = false);
    const Bitmap& image() const {
        return image_;
    }
    const DotMatrixFrame& frame() const {
        return frame_;
    }

private:
    std::string startup_version_ = "1.52P";
    DotMatrixRenderer renderer_;
    VideoRenderer video_renderer_;
    DmdAnimationPlayer animations_;
    DotMatrixFrame frame_;
    Bitmap image_;
    std::uint32_t accumulated_ = 0, score_counter_ = 0, grid_counter_ = 0;
    bool prepared_ = false;
    char ball_character_ = 'X'; // Original shared "BALL X" caption, 0x42b204.
    enum class Page {
        none,
        warning,
        power,
        arena,
        lock,
        unlit_hole,
        loop_combo,
        mission,
        scoring,
        video,
        lock_award,
        explosion,
        ultimate,
        serve,
        entry,
        record_result,
        match,
        status,
        effects_volume,
        bonus
    };
    struct PageRequest {
        Page kind = Page::none;
        unsigned remaining = 0;
        int priority = 0;
    } page_;
    int warning_ = 0;
    int power_level_ = 0;
    DotMatrixRenderer::ArenaPage arena_;
    rules::LoopComboPage loop_combo_page_ = rules::LoopComboPage::loops;
    int loop_combo_count_ = 0;
    rules::ScoreValue loop_combo_amount_;
    DotMatrixRenderer::BonusPage bonus_values_;
    rules::MissionPage mission_page_ = rules::MissionPage::progress;
    rules::MissionDisplayValues mission_values_;
    rules::ScoringPage scoring_page_ = rules::ScoringPage::award;
    DotMatrixRenderer::ScoringValues scoring_values_;
    rules::VideoPage video_page_ = rules::VideoPage::instructions;
    rules::LockPage lock_page_ = rules::LockPage::choice;
    DotMatrixRenderer::LockValues lock_values_;
    rules::ExplosionPage explosion_page_ = rules::ExplosionPage::superjets;
    rules::UltimatePage ultimate_page_ = rules::UltimatePage::introduction;
    rules::UltimateDisplayValues ultimate_values_;
    rules::ServePage serve_page_ = rules::ServePage::player;
    DotMatrixRenderer::ServeValues serve_values_;
    DotMatrixRenderer::EntryValues entry_values_;
    rules::RecordPage record_page_ = rules::RecordPage::initials;
    DotMatrixRenderer::RecordValues record_values_;
    DotMatrixRenderer::MatchValues match_values_;
    DotMatrixRenderer::StatusValues status_values_;
    int status_selection_ = 0;
    unsigned effects_volume_ = 127;
    AttractDisplay attract_;
    AttractValues attract_values_;
    std::uint32_t video_total_ = 0;
    bool video_perfect_ = false;
    DotMatrixFrame render_page(const DotMatrixFrame& backdrop);
    // Rejected text requests must not overwrite their associated value fields.
    bool accept_page_request(int priority, bool replace = true);
};
} // namespace web
