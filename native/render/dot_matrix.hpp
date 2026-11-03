#pragma once
#include "../rules/scoring.hpp"
#include "../rules/bonus.hpp"
#include "../rules/loop_combos.hpp"
#include "../rules/missions.hpp"
#include "../rules/lock_awards.hpp"
#include "../rules/explosion.hpp"
#include "../rules/ultimate.hpp"
#include "../rules/serve.hpp"
#include "../rules/record_entry.hpp"
#include "../rules/score_records.hpp"

namespace web {
struct DotMatrixFrame {
    static constexpr int width = 128, height = 32;
    static constexpr std::size_t pixel_count = width * height;
    std::array<std::uint8_t, pixel_count> pixels{};
};

// Original font descriptors (0x41cc10), transparent glyph blitter (0x41c308),
// score formatting (0x41c540) and active score page (0x41cf10).
class DotMatrixRenderer {
public:
    enum class Alignment {
        left = -1,
        centre = 0,
        right = 1,
        positioned = 2
    };
    struct ScorePage {
        std::array<rules::ScoreValue, 4> scores{};
        std::array<int, 4> balls{};
        unsigned player = 0, players = 1;
        int credits = 0;
        std::uint32_t counter = 0;
        bool repeated_simulation = false;
        bool started = true;
        char ball_character = 0; // Optional retained caption supplied by DmdDisplay.
    };
    struct BonusPage {
        rules::BonusDisplayState display;
        std::int32_t loops = 0, power_levels = 0, combos = 0, multiplier = 1, timer = 0, held = 0, credits = 0;
    };
    struct ArenaPage {
        int ramps = 0, lock_at = 1, extra_ball_at = 20;
    };
    struct ScoringValues {
        rules::ScoreValue amount;
        std::uint32_t ramp_value = 0, skillshot = 0;
        std::int32_t superjet_total = 0;
        bool super_skillshot = false;
    };
    struct LockValues {
        int selection = 0, seconds = 0, remaining = 0;
    };
    struct ServeValues {
        unsigned player = 0;
        bool replay_awarded = false, buyins = false;
        rules::ScoreValue replay{50000000, 0}, high_score{0, 1}, buyin_high_score{80000000, 0};
    };
    struct EntryValues {
        unsigned player = 0, cursor = 0;
        std::uint32_t held_actions = 0;
        std::array<std::uint8_t, 3> initials{'A', 'A', 'A'};
    };
    struct RecordValues {
        unsigned player = 0, rank = 0;
        int combos = 0, loops = 0;
        rules::ScoreValue score;
    };
    struct MatchValues {
        unsigned players = 1;
        int digit = 0, stage = 0;
        std::array<std::uint32_t, 4> score_units{};
    };
    struct StatusValues {
        int multiplier = 1, held = 0, extra_balls = 0, extra_balls_lit = 0, loop_target = 10, spheres = 0;
    };
    explicit DotMatrixRenderer(const std::filesystem::path& assets, bool fit_hideout_caption = false);
    void text(DotMatrixFrame& frame, std::string_view value, unsigned font, int x, int y) const;
    int text_width(std::string_view value, unsigned font) const;
    void score(DotMatrixFrame& frame, rules::ScoreValue value, unsigned font, int x, int y, Alignment alignment) const;
    DotMatrixFrame grid_frame(std::uint32_t counter) const;
    DotMatrixFrame startup_page(std::string_view version = "1.52P") const;
    DotMatrixFrame exit_page(const rules::ScoreRecord&) const; // Original final 0x420acc caller.
    DotMatrixFrame score_page(const ScorePage& page, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame bonus_page(const BonusPage& page, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame warning_page(int warning, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame power_page(int level, std::uint32_t counter, const DotMatrixFrame* background = nullptr) const;
    DotMatrixFrame arena_page(const ArenaPage& page, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame lock_page(std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame lock_award_page(rules::LockPage page, const LockValues& values, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame explosion_page(rules::ExplosionPage page, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame ultimate_page(rules::UltimatePage page, const rules::UltimateDisplayValues&, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame serve_page(rules::ServePage page, const ServeValues&, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame unlit_hole_page(std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame loop_combo_page(rules::LoopComboPage page, int count, rules::ScoreValue amount, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame mission_page(rules::MissionPage page, const rules::MissionDisplayValues& values, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame scoring_page(rules::ScoringPage page, const ScoringValues& values, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame record_entry_page(const EntryValues&, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame record_result_page(rules::RecordPage, const RecordValues&, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame match_page(const MatchValues&, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame status_page(int selection, const StatusValues&, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    DotMatrixFrame effects_volume_page(unsigned volume, std::uint32_t counter, const DotMatrixFrame* backdrop = nullptr) const;
    // Modern presentation of the original four intensities; simulation pixels
    // remain independent of the LED shape, client size and Windows text APIs.
    static Bitmap leds(const DotMatrixFrame& frame, int scale = 4);

private:
    struct Font {
        Bytes pixels;
        int width, height;
    };
    std::array<Font, 7> fonts_;
    Bytes grid_;
    bool fit_hideout_caption_;
    const Font& font(unsigned index) const;
    static int glyph_width(unsigned character, unsigned font, int width);
    static std::string two_digits(int value);
    void text_with_bounds(DotMatrixFrame& frame, std::string_view value, unsigned font, int x, int y, bool linear_rows) const;
};
} // namespace web
