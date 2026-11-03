#include "dot_matrix.hpp"
#include <algorithm>
#include <stdexcept>

namespace web {
DotMatrixRenderer::DotMatrixRenderer(const std::filesystem::path& assets, bool fit_hideout_caption) :
    fit_hideout_caption_(fit_hideout_caption) {
    constexpr std::array<std::array<int, 2>, 7> dimensions = {
        {{8, 8}, {8, 16}, {4, 6}, {11, 32}, {8, 12}, {8, 32}, {7, 12}}
    };
    for (std::size_t i = 0; i < fonts_.size(); ++i) {
        fonts_[i] = {read_file(assets / "MATRIX" / "FONTS" / ("FONT" + std::to_string(i + 1) + ".BIT")), dimensions[i][0], dimensions[i][1]};
    }
    grid_ = read_file(assets / "MATRIX" / "FONTS" / "GRID.BIT");
    if (grid_.size() != 4 * DotMatrixFrame::pixel_count)
        throw std::runtime_error("Invalid original DMD grid length");
}
const DotMatrixRenderer::Font& DotMatrixRenderer::font(unsigned index) const {
    if (index == 0 || index > fonts_.size())
        throw std::out_of_range("Unknown original DMD font");
    return fonts_[index - 1];
}
int DotMatrixRenderer::glyph_width(unsigned character, unsigned index, int width) {
    const bool half = (character == ',' || (character == ' ' && index != 3)) && index != 6;
    return half ? (width + 1) / 2 : width;
}
int DotMatrixRenderer::text_width(std::string_view value, unsigned index) const {
    const auto& face = font(index);
    int width = 0;
    for (unsigned char character : value)
        width += glyph_width(character, index, face.width);
    return width;
}
void DotMatrixRenderer::text(DotMatrixFrame& frame, std::string_view value, unsigned index, int x, int y) const {
    text_with_bounds(frame, value, index, x, y, false);
}
void DotMatrixRenderer::text_with_bounds(DotMatrixFrame& frame, std::string_view value, unsigned index, int x, int y, bool linear_rows) const {
    const auto& face = font(index);
    constexpr unsigned first_character = 32, glyph_columns = 16;
    for (unsigned char character : value) {
        if (character < first_character)
            throw std::out_of_range("DMD character precedes original font atlas");
        const auto glyph = unsigned(character) - first_character;
        const auto source = (glyph % glyph_columns) * face.width + (glyph / glyph_columns) * glyph_columns * face.width * face.height;
        const auto advance = glyph_width(character, index, face.width);
        if (source + (face.height - 1) * glyph_columns * face.width + advance > face.pixels.size())
            throw std::out_of_range("DMD character outside original font atlas");
        for (int row = 0; row < face.height; ++row) {
            for (int column = 0; column < advance; ++column) {
                const auto intensity = face.pixels[source + row * glyph_columns * face.width + column];
                const auto destination = (y + row) * DotMatrixFrame::width + x + column;
                const bool inside = linear_rows ? destination >= 0 && destination < int(frame.pixels.size())
                                                : x + column >= 0 && x + column < DotMatrixFrame::width && y + row >= 0 && y + row < DotMatrixFrame::height;
                if (intensity < 4 && inside)
                    frame.pixels[std::size_t(destination)] = intensity;
            }
        }
        x += advance;
    }
}
void DotMatrixRenderer::score(DotMatrixFrame& frame, rules::ScoreValue value, unsigned index, int x, int y, Alignment alignment) const {
    // The original accepts one extra low-word carry. Valid game scores retain
    // at most three high digits; reject corrupt presentation input safely.
    if (value.units >= rules::ScoreValue::radix) {
        value.units -= rules::ScoreValue::radix;
        ++value.rollovers;
    }
    if (value.units >= rules::ScoreValue::radix || value.rollovers > 999)
        throw std::out_of_range("DMD score exceeds original formatter");
    auto formatted = value.text();
    if (value.units == 0 && value.rollovers == 0)
        formatted = "00"; // Original formatter always includes its trailing zero.
    // 0x41c540 reserves half a cell for every separator, even for face six,
    // whose glyph blitter uses a full-width comma. Keep that original mismatch.
    const auto separators = int(std::count(formatted.begin(), formatted.end(), ','));
    const auto& face = font(index);
    const auto width = (int(formatted.size()) - separators) * face.width + separators * ((face.width + 1) / 2);
    if (alignment == Alignment::left)
        x = 4;
    else if (alignment == Alignment::centre)
        x = DotMatrixFrame::width / 2 - width / 2;
    else if (alignment == Alignment::right)
        x = (index == 7 ? 124 : 125) - width;
    text(frame, formatted, index, x, y);
}
DotMatrixFrame DotMatrixRenderer::exit_page(const rules::ScoreRecord& champion) const {
    DotMatrixFrame frame;
    text(frame, "GRAND CHAMPION", 5, 10, 3);
    std::string initials;
    for (auto character : champion.initials) {
        if (!character)
            break;
        initials += char(character);
    }
    text(frame, initials, 5, 4, 17);
    score(frame, champion.score, champion.score.rollovers < 10 ? 5 : 7, 32, 17, Alignment::right);
    return frame;
}
DotMatrixFrame DotMatrixRenderer::score_page(const ScorePage& page, const DotMatrixFrame* backdrop) const {
    if (page.players < 1 || page.players > page.scores.size() || page.player >= page.players)
        throw std::out_of_range("Invalid DMD score page player");
    auto frame = backdrop ? *backdrop : grid_frame(page.counter);
    score(frame, page.scores[page.player], 2, 0, 8, Alignment::centre);
    for (unsigned player = 0; player < page.players; ++player) {
        if (player != page.player)
            score(frame, page.scores[player], 3, 0, player < 2 ? 1 : 25, player % 2 ? Alignment::right : Alignment::left);
    }
    const bool ball_visible = ((page.counter + 1) & 0x30) != 0 || page.players != 4;
    if (ball_visible) {
        const auto ball = "BALL " + (page.ball_character ? std::string(1, page.ball_character) : std::to_string(page.balls[page.player]));
        text(frame, ball, 3, page.player % 2 ? 101 : 4, page.player < 2 ? 1 : 25);
    }
    if (page.players != 4 || !ball_visible) {
        const auto corner = page.players == 4 ? page.player : 3;
        const auto credits = "CREDITS " + std::to_string(page.credits) + (page.repeated_simulation ? "." : " ");
        text(frame, credits, 3, corner % 2 ? 89 : 4, corner < 2 ? 1 : 25);
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::grid_frame(std::uint32_t counter) const {
    DotMatrixFrame frame;
    std::copy_n(grid_.begin() + (counter % 4) * DotMatrixFrame::pixel_count, frame.pixels.size(), frame.pixels.begin());
    return frame;
}
DotMatrixFrame DotMatrixRenderer::startup_page(std::string_view version) const {
    DotMatrixFrame frame;
    // Original mode -1 layout. The atlas has symbols in lowercase ASCII slots;
    // edition versions use the original uppercase lettering for display.
    std::string caption(version);
    for (char& character : caption)
        if (character >= 'a' && character <= 'z')
            character = char(character - 'a' + 'A');
    text(frame, "TESTING...", 1, 8, 5);
    text(frame, "VERSION " + caption, 1, 8, 19);
    return frame;
}
std::string DotMatrixRenderer::two_digits(int value) {
    std::string result(2, '0');
    result[0] = char('0' + (value / 10) % 10);
    result[1] = char('0' + value % 10);
    return result;
}
DotMatrixFrame DotMatrixRenderer::bonus_page(const BonusPage& page, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    const auto amount = [&] {
        constexpr unsigned radix = rules::ScoreValue::radix;
        score(frame, {page.display.value % radix, (page.display.value / radix) % 10}, 2, 0, 16, Alignment::centre);
    };
    switch (page.display.stage) {
        case -1:
            text(frame, "BONUS SCORE", 2, 22, 8);
            break;
        case 0:
        case 1:
            if (page.display.stage == 1)
                amount();
            text(frame, two_digits(page.loops) + " LOOPS", 5, 34, 2);
            break;
        case 2:
        case 3:
            if (page.display.stage == 3)
                amount();
            text(frame, two_digits(page.power_levels) + " POWER LEVELS", 5, 8, 2);
            break;
        case 4:
        case 5:
            if (page.display.stage == 5)
                amount();
            text(frame, two_digits(page.combos) + " COMBOS", 5, 30, 2);
            break;
        case 7:
            switch (page.multiplier) {
                case 1:
                case 2:
                case 4:
                case 6:
                case 8:
                    text(frame, "X" + std::to_string(page.multiplier), 2, 56, 16);
                    break;
                default:
                    text(frame, "X10", 2, 52, 16);
                    break;
            }
            [[fallthrough]];
        case 6:
        case 8:
            text(frame, "TOTAL BONUS", 5, 22, 2);
            break;
        case 9:
            if (!(std::uint32_t(page.held) & std::uint32_t(page.timer < 0x4000))) {
                amount();
                text(frame, "TOTAL BONUS", 5, 22, 2);
            } else if ((counter + 1) & 1)
                text(frame, "BONUS HELD", 5, 26, 8);
            break;
        case 11:
            text(frame, "CREDITS " + std::to_string(page.credits % 10) + "   PRESS", 5, 4, 3);
            text(frame, "C TO CONTINUE    " + std::to_string(page.display.continue_seconds % 10), 5, 4, 17);
            break;
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::warning_page(int warning, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    switch (warning) {
        case -2:
            frame.pixels.fill(0);
            break;
        case -1:
            if ((counter + 1) & 2)
                text(frame, "TILT", 5, 48, 8);
            break;
        case 0:
            text(frame, "DANGER", 5, 40, 3);
            text(frame, "DANGER", 5, 40, 17);
            break;
        case 1:
            text(frame, "DANGER", 5, 40, 8);
            break;
        case 2:
            text(frame, "WARNING", 5, 36, 8);
            break;
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::power_page(int level, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    struct Label {
        const char* text;
        int x;
    };
    constexpr std::array<Label, 6> labels = {
        {{"SMALL POINTS", 16}, {"EXPLOSION READY", 8}, {"SUPERJETS ACTIVE", 2}, {"QUICK SHOT READY", 4}, {"VIDEO MODE READY", 4}, {"SUPER BALLRESCUE", 2}}
    };
    if (level >= 1 && level <= int(labels.size())) {
        const auto& label = labels[unsigned(level - 1)];
        text(frame, label.text, 5, label.x, 10);
    } else if (level == 7) {
        text(frame, "SUPER LAUNCHER", 5, 8, 3);
        text(frame, "READY", 5, 40, 17);
    } else if (level != 8) {
        score(frame, {std::uint32_t(level) * 100000u, 0}, 4, 0, 0, Alignment::centre);
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::arena_page(const ArenaPage& page, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    text(frame, two_digits(page.ramps) + " RAMPS", 5, 2, 3);
    const auto next = page.lock_at < page.extra_ball_at ? "LOCK LIT AT " + two_digits(page.lock_at) : "EXTRA BALL AT " + two_digits(page.extra_ball_at);
    text(frame, next, 5, 2, 17);
    return frame;
}
DotMatrixFrame DotMatrixRenderer::lock_page(std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    text(frame, "LOCK IS LIT", 5, 24, 10);
    return frame;
}
DotMatrixFrame DotMatrixRenderer::explosion_page(rules::ExplosionPage page, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    switch (page) {
        case rules::ExplosionPage::superjets:
            text(frame, "SUPERJETS ACTIVE", 5, 2, 10);
            break;
        case rules::ExplosionPage::quick_shot:
            text(frame, "QUICK SHOT READY", 5, 4, 10);
            break;
        case rules::ExplosionPage::video_ready:
            text(frame, "VIDEO MODE READY", 5, 4, 10);
            break;
        case rules::ExplosionPage::super_rescue:
            text(frame, "SUPER BALLRESCUE", 5, 2, 10);
            break;
        case rules::ExplosionPage::outlanes:
            text(frame, "OUTLANES ARE LIT", 5, 4, 10);
            break;
        case rules::ExplosionPage::saver:
            text(frame, "30 SECOND SAVER", 5, 8, 10);
            break;
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::unlit_hole_page(std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    text(frame, "SHOOT RAMPS TO", 5, 10, 3);
    text(frame, "RELIGHT MISSION", 5, 6, 17);
    return frame;
}
DotMatrixFrame DotMatrixRenderer::lock_award_page(rules::LockPage page, const LockValues& values, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    const auto progress = [&](int remaining) {
        if (remaining < 1 || remaining > 3)
            return;
        text(frame, remaining == 1 ? "LOCK 1 MORE BALL" : "LOCK " + std::to_string(remaining) + " MORE BALLS", 5, remaining == 1 ? 6 : 2, 3);
        text(frame, "FOR FAST FRENZY", 5, 8, 17);
    };
    if (page == rules::LockPage::no_jackpot) {
        text(frame, "SHOOT DROPS TO", 5, 10, 3);
        text(frame, "LIGHT JACKPOT", 5, 14, 17);
    } else if (page == rules::LockPage::progress)
        progress(values.remaining);
    else if (values.seconds > 8) {
        text(frame, "PICK A FRENZY", 5, 16, 8);
    } else if (values.selection < 4) {
        for (int i = 0; i < 4; ++i) {
            if (i == values.selection && !((counter + 1) & 1))
                continue;
            std::string icons;
            for (int glyph = 0; glyph < 4; ++glyph)
                icons += char(32 + i * 4 + glyph);
            text(frame, icons, 6, i * 32, 0);
        }
        if (values.seconds != -1)
            text(frame, std::string(1, char('0' + values.seconds % 10)), 5, 120, 20);
    } else if (values.selection < 7)
        progress(values.selection - 3);
    else if (values.selection == 7 && ((counter + 1) & 1))
        text(frame, "MYSTERY SELECTED", 5, 2, 8);
    return frame;
}
DotMatrixFrame DotMatrixRenderer::loop_combo_page(rules::LoopComboPage page, int count, rules::ScoreValue amount, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    switch (page) {
        case rules::LoopComboPage::loops:
            text(frame, " " + two_digits(count), 4, 1, 0);
            break;
        case rules::LoopComboPage::mania_remaining:
            text(frame, "COMBO MANIA IN " + std::string(1, char('0' + count % 10)), 5, 4, 10);
            break;
        case rules::LoopComboPage::mania_ready:
            text(frame, "COMBO MANIA IS", 5, 10, 3);
            text(frame, "READY", 5, 40, 17);
            break;
        case rules::LoopComboPage::mania_score:
            score(frame, amount, 4, 0, 0, Alignment::centre);
            break;
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::mission_page(rules::MissionPage page, const rules::MissionDisplayValues& values, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    const auto progress = [&] {
        text(frame, two_digits(std::min(values.percent, 99)) + "%", 5, 104, 20);
        text(frame, two_digits(values.seconds), 5, 1, 20);
    };
    const auto introduction = [&](std::string_view title, std::string_view first, std::string_view second) {
        text(frame, title, 5, 2, 2);
        text(frame, first, 3, 2, 16);
        text(frame, second, 3, 2, 24);
    };
    switch (page) {
        case rules::MissionPage::reactor_intro:
            introduction("REACTOR CRITICAL", "SHOOT DROP TARGETS TO INSERT", "SAFETY RODS");
            break;
        case rules::MissionPage::hunt_intro:
            introduction("HUNT DOWN", "SHOOT LIT SINK HOLES TO FIND", "AND ELIMINATE TARGET");
            break;
        case rules::MissionPage::ammo_intro:
            introduction("DESTROY AMMODUMP", "SHOOT JET BUMPERS TO DESTROY", "AMMO DUMP");
            break;
        case rules::MissionPage::skyscraper_intro:
            introduction("SKY SCRAPER RAID", "SHOOT SPINNERS TO GO UP FLOORS", "CENTRE SHOT COLLECTS AWARD");
            break;
        case rules::MissionPage::shuttle_intro:
            introduction("STOP THE SHUTTLE", "SHOOT RAMPS TO FIRE GUN AT", "SHUTTLE");
            break;
        case rules::MissionPage::biker_intro:
            introduction("BIKER CHALLENGE", "SHOOT LIT LOOPS AND RAMPS TO", "TAKE OUT BIKER");
            break;
        case rules::MissionPage::progress:
            progress();
            break;
        case rules::MissionPage::total:
            if (remaining < 24) {
                text(frame, "MISSION TOTAL", 5, 14, 2);
                const auto amount = (std::uint32_t(values.total) * 100000u) % rules::ScoreValue::radix;
                score(frame, {amount, (std::uint32_t(values.total) / 1000) % 10}, 2, 0, 16, Alignment::centre);
            }
            break;
        case rules::MissionPage::cycle_complete:
            if (remaining > 18) {
                text(frame, "ALL MISSIONS", 5, 16, 3);
                text(frame, "COMPLETE", 5, 32, 17);
            } else if ((counter + 1) & 1) {
                const auto amount = std::uint32_t(values.rounds) * 25000000u;
                score(frame, {amount % rules::ScoreValue::radix, amount / rules::ScoreValue::radix}, 4, 0, 0, Alignment::centre);
            }
            break;
        case rules::MissionPage::skyscraper_award:
            if ((counter + 1) & 0x18) {
                constexpr std::array<unsigned, 5> awards = {500000, 1000000, 2500000, 5000000, 10000000};
                if (values.floor >= 1 && values.floor <= int(awards.size()))
                    score(frame, {awards[unsigned(values.floor - 1)] * (std::uint32_t(values.rounds) + 1), 0}, 2, 0, 2, Alignment::centre);
                else if (values.floor == 6)
                    text(frame, "EXTRA BALL", 2, 26, 2);
            }
            progress();
            break;
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::scoring_page(rules::ScoringPage page, const ScoringValues& values, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    switch (page) {
        case rules::ScoringPage::award:
        case rules::ScoringPage::flashing_award:
            if (page == rules::ScoringPage::award || ((counter + 1) & 1))
                score(frame, values.amount, 4, 0, 0, Alignment::centre);
            break;
        case rules::ScoringPage::orbit_award:
            text(frame, "  SHOOT ORBIT FOR", 5, 2, 2);
            score(frame, values.amount, 2, 0, 16, Alignment::centre);
            break;
        case rules::ScoringPage::hideout_award:
            // The shipped phrase is 128 columns wide at x=2. Retain its two
            // columns spilling into the next row, within the owned frame.
            text_with_bounds(frame, "SHOOT HIDEOUT FOR", 1, fit_hideout_caption_ ? 0 : 2, 4, !fit_hideout_caption_);
            score(frame, {values.amount.units, 0}, 2, 0, 14, Alignment::centre);
            break;
        case rules::ScoringPage::ramp_value:
            text(frame, "JACKPOT GROWS", 5, 16, 2);
            score(frame, {(values.ramp_value / 10) % rules::ScoreValue::radix, values.ramp_value / 1000000000}, 2, 0, 16, Alignment::centre);
            break;
        case rules::ScoringPage::superjet_total: {
            text(frame, "SUPER JETS SCORE", 5, 4, 2);
            const auto amount = std::uint32_t(values.superjet_total) * 100000u;
            score(frame, {amount % rules::ScoreValue::radix, (amount / rules::ScoreValue::radix) % 10}, 2, 0, 16, Alignment::centre);
            break;
        }
        case rules::ScoringPage::skillshot:
            if (remaining > 16)
                text(frame, values.super_skillshot ? "SUPER SKILL SHOT" : "SKILL SHOT", 2, values.super_skillshot ? 4 : 24, 8);
            else if (((counter + 1) & 1) || !values.super_skillshot)
                score(frame, {values.skillshot % rules::ScoreValue::radix, values.skillshot / rules::ScoreValue::radix}, 4, 0, 0, Alignment::centre);
            break;
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::ultimate_page(rules::UltimatePage page, const rules::UltimateDisplayValues& values, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    switch (page) {
        case rules::UltimatePage::reserve:
            text(frame, two_digits(values.reserves) + " RESERVE BALLS", 5, 4, 10);
            break;
        case rules::UltimatePage::introduction:
            frame = {};
            switch (values.introduction) {
                case 1:
                    text(frame, "ULTRA JACKPOT", 5, 14, 3);
                    text(frame, "SCORED!", 5, 36, 17);
                    break;
                case 2:
                    text(frame, "COMBO MANIA", 5, 22, 3);
                    text(frame, "PLAYED!", 5, 36, 17);
                    break;
                case 3:
                    text(frame, "POWER LEVEL 8", 5, 16, 3);
                    text(frame, "REACHED!", 5, 32, 17);
                    break;
                case 4:
                    text(frame, "BONUS", 5, 44, 3);
                    text(frame, "MAXED OUT!", 5, 26, 17);
                    break;
                case 5:
                    text(frame, "ALL MISSIONS", 5, 16, 3);
                    text(frame, "COMPLETE!", 5, 28, 17);
                    break;
                case 6:
                    text(frame, two_digits(values.spheres) + " SHOWDOWN", 5, 22, 3);
                    text(frame, "SPHERES", 5, 36, 17);
                    break;
                case 7:
                    text(frame, "ALL SWITCHES", 5, 18, 3);
                    text(frame, "SCORE", 5, 44, 17);
                    break;
                case 8:
                case 9:
                    if ((counter + 1) & 1)
                        score(frame, {std::uint32_t(values.spheres) * 100000u, 0}, 4, 0, 0, Alignment::centre);
                    break;
            }
            break;
        case rules::UltimatePage::blackout:
            frame = {};
            break;
        case rules::UltimatePage::total:
            text(frame, "SHOWDOWN TOTAL", 5, 10, 2);
            score(frame, {std::uint32_t(values.total % 1000) * 100000u, std::uint32_t(values.total / 1000) % 10}, 2, 0, 16, Alignment::centre);
            break;
        case rules::UltimatePage::victory_award:
            if ((counter + 1) & 1) {
                const auto amount = std::uint32_t(values.rounds) * rules::ScoreValue::radix;
                score(frame, {amount % rules::ScoreValue::radix, (amount / rules::ScoreValue::radix) % 10}, 4, 0, 0, Alignment::centre);
            }
            break;
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::effects_volume_page(unsigned volume, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    if (volume > 127)
        throw std::out_of_range("Effects volume must be between 0 and 127");
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    text(frame, "SFX", 1, 6, 12);
    constexpr unsigned x = 56, y = 9, width = 68, height = 10;
    const auto fill = volume * (width - 2) / 127;
    for (const auto row : {y, y + height - 1})
        std::fill_n(frame.pixels.begin() + row * DotMatrixFrame::width + x, width, 2);
    for (unsigned row = y + 1; row < y + height - 1; ++row) {
        frame.pixels[row * DotMatrixFrame::width + x] = 2;
        frame.pixels[row * DotMatrixFrame::width + x + width - 1] = 2;
        std::fill_n(frame.pixels.begin() + row * DotMatrixFrame::width + x + 1, fill, 3);
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::status_page(int selection, const StatusValues& values, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    switch (selection) {
        case 0: {
            text(frame, "BONUS MULTIPLIER", 5, 2, 3);
            const int x = values.held ? 16 : 44;
            if (values.held)
                text(frame, "AND HELD", 5, 56, 17);
            if (values.multiplier == 1 || values.multiplier == 2 || values.multiplier == 4 || values.multiplier == 6 || values.multiplier == 8)
                text(frame, "AT X" + std::to_string(values.multiplier), 5, x, 17);
            else if (values.multiplier == 10)
                text(frame, "AT X10", 5, x - 8, 17);
            else
                text(frame, "MAXED", 5, x - 4, 17);
            break;
        }
        case 1:
            text(frame, std::string(1, char('0' + values.extra_balls % 10)) + " EXTRA BALLS", 5, 2, 3);
            text(frame, std::string(1, char('0' + values.extra_balls_lit % 10)) + " EXTRA BALLS LIT", 5, 2, 17);
            break;
        case 2: {
            std::string digits{char('0' + values.loop_target / 10), char('0' + values.loop_target % 10)};
            text(frame, digits + " LOOPS TO LIGHT", 5, 2, 3);
            text(frame, "EXTRA BALL", 5, 2, 17);
            break;
        }
        case 3:
            text(frame, two_digits(values.spheres) + " SHOWDOWN", 5, 22, 3);
            text(frame, "SPHERES", 5, 36, 17);
            break;
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::serve_page(rules::ServePage page, const ServeValues& values, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    if (page == rules::ServePage::player || remaining > 24) {
        if (values.player < 4)
            text(frame, "PLAYER " + std::to_string(values.player + 1) + " UP", 2, 22, 8);
    } else {
        const auto value = !values.replay_awarded ? values.replay : !values.buyins ? values.high_score : values.buyin_high_score;
        text(frame, !values.replay_awarded ? "REPLAY AT" : !values.buyins ? "HIGHEST SCORE" : "TOP BUYIN SCORE", 5, !values.replay_awarded ? 30 : !values.buyins ? 14 : 8, 2);
        if ((counter + 1) & 1)
            score(frame, value, 2, 0, 16, Alignment::centre);
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::record_entry_page(const EntryValues& values, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    if (values.cursor < 3) {
        text(frame, "ENTER INITIALS", 5, 10, 3);
        auto caption = "PLAYER " + std::to_string(values.player + 1) + "  ";
        for (const auto value : values.initials)
            caption += char(value);
        if (!((counter + 1) & 6) && !values.held_actions)
            caption[10 + values.cursor] = '_';
        text(frame, caption, 5, 18, 17);
    } else if ((counter + 1) & 1) {
        std::string caption;
        for (unsigned i = 0; i < 3; ++i) {
            if (i)
                caption += ' ';
            caption += char(values.initials[i]);
        }
        text(frame, caption, 2, 48, 8);
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::record_result_page(rules::RecordPage page, const RecordValues& values, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    const bool flash = (counter + 1) & 1;
    if (page == rules::RecordPage::score) {
        if (remaining > 24) {
            text(frame, "GREAT SCORE!!!", 5, 10, 2);
            if (flash)
                text(frame, "PLAYER " + std::to_string(values.player + 1), 5, 34, 17);
        } else {
            if (values.rank == 0)
                text(frame, "GRAND CHAMPION", 5, 10, 2);
            else if (values.rank <= 4)
                text(frame, "HIGH SCORE " + std::to_string(values.rank), 5, 20, 2);
            else if (values.rank <= 8)
                text(frame, "BUYIN SCORE " + std::to_string(values.rank - 4), 5, 16, 2);
            else if (values.rank <= 12)
                text(frame, "TODAYS NUMBER " + std::to_string(values.rank - 8), 5, 8, 2);
            if (flash)
                score(frame, values.score, 2, 0, 16, Alignment::centre);
        }
    } else if (remaining > 24) {
        text(frame, "WELL PLAYED!!!", 5, 10, 3);
        text(frame, "PLAYER " + std::to_string(values.player + 1), 5, 34, 17);
    } else if (page == rules::RecordPage::combo) {
        text(frame, "COMBO CHAMPION", 5, 10, 3);
        if (flash)
            text(frame, two_digits(values.combos) + " COMBOS", 5, 30, 17);
    } else if (page == rules::RecordPage::loop) {
        text(frame, "LOOP CHAMPION", 5, 14, 3);
        if (flash)
            text(frame, two_digits(values.loops) + " LOOPS", 5, 34, 17);
    }
    return frame;
}
DotMatrixFrame DotMatrixRenderer::match_page(const MatchValues& values, unsigned remaining, std::uint32_t counter, const DotMatrixFrame* backdrop) const {
    auto frame = backdrop ? *backdrop : grid_frame(counter);
    text(frame, "MATCH", 5, 44, 2);
    if (remaining < 116)
        score(frame, {std::uint32_t(values.digit), 0}, 2, 0, 16, Alignment::centre);
    if (remaining < 122) {
        for (unsigned i = std::min(values.players, 4u); i-- > 0;) {
            const auto digit = values.score_units[i] % 10;
            if (values.stage == 2 && !((counter + 1) & 1) && digit == unsigned(values.digit))
                continue;
            score(frame, {digit, 0}, 5, 0, i < 2 ? 2 : 18, i % 2 ? Alignment::right : Alignment::left);
        }
    }
    return frame;
}
Bitmap DotMatrixRenderer::leds(const DotMatrixFrame& frame, int scale) {
    if (scale < 1 || scale > 16)
        throw std::out_of_range("Invalid DMD LED scale");
    Bitmap image{DotMatrixFrame::width * scale, DotMatrixFrame::height * scale, {}};
    constexpr std::array<std::uint32_t, 4> palette = {0x160c04, 0x693107, 0xc7660b, 0xffb52e};
    image.pixels.resize(std::size_t(image.width) * image.height, 0x070503);
    for (int y = 0; y < DotMatrixFrame::height; ++y) {
        for (int x = 0; x < DotMatrixFrame::width; ++x) {
            const auto value = frame.pixels[std::size_t(y) * DotMatrixFrame::width + x];
            if (value >= palette.size())
                throw std::out_of_range("Invalid DMD pixel intensity");
            for (int row = 0; row < scale; ++row)
                for (int column = 0; column < scale; ++column)
                    if (scale <= 2 || (row < scale - 1 && column < scale - 1))
                        image.pixels[std::size_t(y * scale + row) * image.width + x * scale + column] = palette[value];
        }
    }
    return image;
}
} // namespace web
