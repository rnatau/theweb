#include "attract_display.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web {
using namespace physics::integer;
namespace {
std::string initials(const std::array<std::uint8_t, 4>& value) {
    std::string result;
    for (auto c : value) {
        if (!c)
            break;
        result += char(c);
    }
    return result;
}
std::string count_caption(std::uint32_t value, std::string_view caption) {
    const auto signed_value = bits(value);
    return std::string{char('0' + (signed_value / 10) % 10), char('0' + signed_value % 10)} + std::string(caption);
}
char code_digit(std::uint32_t value) {
    value &= 31;
    return value == 24 ? 'Y' : value == 28 ? 'Z' : char(value < 10 ? '0' + value : 'A' + value - 10);
}
struct Credit {
    std::string_view title;
    std::array<std::string_view, 3> names;
};
constexpr Credit credits[] = {
    {"GAME DESIGN",     {"ADRIAN BARRITT", "GRAHAM RICE", "CHAMPIE"}             },
    {"PROGRAMMING",     {"ADRIAN BARRITT", "CHAMPIE", "ADRIAN PAGE"}             },
    {"GRAPHICS",        {"GRAHAM RICE"}                                          },
    {"GRAPHICS",        {"GRAHAM RICE"}                                          },
    {"DOT MATRIX",      {"MINDS EYE"}                                            },
    {"MUSIC",           {"JAKE BURNS", "BRUCE FOXTON"}                           },
    {"VOICE ARTISTS",   {"ADAM LONGWORTH", "MONICA BUFERD", "FRAMBOISE GOMMENDY"}},
    {"AUDIO ENGINEERS", {"PETER KEILY", "ADRIAN PAGE"}                           },
    {"SOUND EFFECTS",   {"ADRIAN PAGE", "MALCOLM AND NAINITA"}                   },
    {"QA MANAGER",      {"ANDY MULLINS"}                                         },
    {"QA TEAM",         {"DARREN THOMPSON", "TRISTRAM DEFRIES", "EDDY LAMBERT"}  },
    {"QA TEAM",         {"SIMON FLYNN", "MAHENDRA SAMPATH"}                      },
    {"PLAY TESTING",    {"RIK YAPP", "ANDREW SCHOFIELD", "MARTIN AYUB"}          },
    {"PLAY TESTING",    {"IAN COTTEE", "GEOFF CAMPBELL", "PHILLIP MORRIS"}       },
    {"PLAY TESTING",    {"CHRIS JORDAN", "SAUL MARCHESE", "GARETH JONES"}        },
    {"PLAY TESTING",    {"TIM GOSLING", "MIKE VAN SLOOTEN", "SIMON HARRISON"}    },
    {"PLAY TESTING",    {"WALT BAYER", "NICK DAWSON", "AND ALL AT EI"}           },
    {"PRODUCED BY",     {"MARK HAVENS AND", "ADRIAN BARRITT"}                    }
};
} // namespace
std::string AttractDisplay::score_code(std::uint32_t kind, const std::array<std::uint8_t, 4>& name, std::uint32_t value, std::uint32_t parameter) {
    const auto letter = [&](unsigned i) { return std::uint32_t(name[i]) - 'A'; };
    auto packed = kind | ((letter(0) | (letter(1) << 5) | (letter(2) << 10)) << 2) | ((parameter & 1023) << 17);
    std::uint32_t checksum = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
        checksum ^= ((value >> shift) ^ (packed >> shift)) & 255;
    constexpr unsigned positions[] = {2, 7, 11, 18, 22, 29};
    for (unsigned i = 0; i < 6; ++i) {
        const auto exchanged = ((value >> positions[i]) ^ (checksum >> i)) & 1;
        value ^= exchanged << positions[i];
        checksum ^= exchanged << i;
    }
    // Original score-sharing code: exchange six checksum bits with the score,
    // then scramble the score and 27-bit kind/initials/parameter payload. These
    // constants are format keys, not addresses or arithmetic tuning values.
    constexpr std::uint32_t score_scramble_key = 0x7f49e31d, payload_scramble_key = 0xa5f2953b, payload_mask = 0x07ffffff;
    value ^= score_scramble_key;
    packed = ((packed ^ payload_scramble_key) & payload_mask) | (checksum << 27);
    const auto encoded = (std::uint64_t(packed) << 32) | value;
    std::string result;
    for (unsigned shift = 0; shift < 60; shift += 5)
        result += code_digit(std::uint32_t(encoded >> shift));
    result += code_digit(checksum >> 1);
    return result;
}
void AttractDisplay::ranking(const DotMatrixRenderer& renderer, DotMatrixFrame& frame, const rules::ScoreRecord& record, int y) {
    renderer.text(frame, initials(record.initials), 5, 4, y);
    state.score_font = bits(record.score.rollovers) < 10 ? 5 : 7;
    renderer.score(frame, record.score, unsigned(state.score_font), 32, y, DotMatrixRenderer::Alignment::right);
}
DotMatrixFrame AttractDisplay::render(const DotMatrixRenderer& renderer, const AttractValues& values, const DotMatrixFrame& backdrop) {
    auto frame = backdrop;
    if (!state.transition) {
        if ((values.held & input_action::both_flippers) == input_action::both_flippers) {
            state.skip = 0;
            state.timer = 240;
            state.page = 100;
        }
        if (!(values.held & input_action::both_flippers))
            state.timer = sub(state.timer, 1);
        if (state.timer < 0 || state.skip) {
            state.transition = 2;
            state.page = add(state.page, 1);
            state.timer = mul(state.duration, 12);
            if (state.page == 16) {
                state.credit_cycles = add(state.credit_cycles, 1);
                if (state.credit_cycles != 2)
                    state.page = 0;
            }
            if (state.page == 34 || state.page == 104) {
                state.credit_cycles = 0;
                state.page = 0;
            }
        }
    } else
        state.transition = sub(state.transition, 1);
    if (state.transition)
        return frame;
    if (state.page == 18)
        state.page = 19;
    if (state.page >= 1 && state.page <= 33)
        state.duration = 2;
    const auto text = [&](std::string_view caption, int x, int y, unsigned font = 5) { renderer.text(frame, caption, font, x, y); };
    const auto title = [&](std::string_view first, int x) {
        text(first, x, 3);
        text("HIGH SCORES", 22, 17);
    };
    switch (state.page) {
        case 0:
            state.transition = state.timer = 0;
            state.duration = 8;
            break;
        case 1:
            text("abcdefghijklm", 14, 8, 2);
            for (int i = 0; i < std::min(4, values.previous_players); ++i)
                renderer.score(frame, values.scores[unsigned(i)], 3, 0, i < 2 ? 1 : 25, i % 2 ? DotMatrixRenderer::Alignment::right : DotMatrixRenderer::Alignment::left);
            break;
        case 2:
            text("CREDITS " + std::to_string(values.credits) + (values.repeated_simulation ? "." : " "), 30, 8);
            break;
        case 3:
            text("REPLAY AT", 30, 2);
            renderer.score(frame, values.replay, 2, 0, 16, DotMatrixRenderer::Alignment::centre);
            state.duration = 3;
            break;
        case 4:
            text("GRAND CHAMPION", 10, 3);
            ranking(renderer, frame, values.records.ordinary()[0], 17);
            break;
        case 5:
            text("COMBO CHAMPION", 10, 3);
            text(initials(values.records.combos().initials), 16, 17);
            text(count_caption(values.records.combos().count, " COMBOS"), 48, 17);
            break;
        case 6:
            text("LOOP CHAMPION", 14, 3);
            text(initials(values.records.loops().initials), 18, 17);
            text(count_caption(values.records.loops().count, " LOOPS"), 50, 17);
            break;
        case 7:
            title("ALL TIME", 34);
            break;
        case 10:
            title("BOUGHT IN", 30);
            break;
        case 13:
            title("TODAYS", 40);
            break;
        case 8:
        case 9:
        case 11:
        case 12:
        case 14:
        case 15: {
            const auto group = (state.page - 8) / 3;
            const auto row = (state.page - 8) % 3 == 0 ? 1u : 3u;
            const auto& table = group == 0 ? values.records.ordinary() : group == 1 ? values.records.buyin() : values.records.session();
            ranking(renderer, frame, table[row], 3);
            ranking(renderer, frame, table[row + 1], 17);
            break;
        }
        case 100:
        case 101:
        case 102:
        case 103: {
            std::string code;
            if (state.page == 100) {
                const auto& record = values.records.ordinary()[0];
                code = score_code(0, record.initials, record.score.units, record.score.rollovers);
                text("GRAND CHAMPION", 10, 3);
            } else if (state.page == 101) {
                const auto& record = values.records.combos();
                code = score_code(3, record.initials, 0, record.count);
                text("COMBO CHAMPION", 10, 3);
            } else if (state.page == 102) {
                const auto& record = values.records.loops();
                code = score_code(2, record.initials, 0, record.count);
                text("LOOP CHAMPION", 14, 3);
            } else {
                const auto& record = values.records.buyin()[1];
                code = score_code(1, record.initials, record.score.units, record.score.rollovers);
                text("BUYIN HIGHSCORE", 6, 3);
            }
            text(code + " ", 12, 17);
            state.duration = 20;
            break;
        }
        default:
            if (state.page >= 16 && state.page <= 33) {
                const auto& credit = credits[state.page - 16];
                text(credit.title, 2, 2, 1);
                for (unsigned i = 0; i < credit.names.size(); ++i)
                    text(credit.names[i], 2, 12 + int(i) * 6, 3);
                if (state.page == 32)
                    state.duration = 4;
            }
            break;
    }
    return frame;
}
} // namespace web
