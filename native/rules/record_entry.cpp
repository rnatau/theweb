#include "record_entry.hpp"
#include "../physics/integer.hpp"
#include <algorithm>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;
using namespace physics::integer;
namespace {
constexpr auto cursor = F::initials_cursor;
constexpr auto initials = S::initials_letters;
constexpr unsigned characters = 3;
constexpr std::uint8_t first_character = 'A', last_character = ']';
} // namespace
std::uint8_t RecordEntryRules::character(unsigned index) const {
    return std::uint8_t(std::uint32_t(session_.initials_letters) >> (index * 8));
}
void RecordEntryRules::character(unsigned index, std::uint8_t value) {
    const auto shift = index * 8;
    session_.initials_letters = bits((std::uint32_t(session_.initials_letters) & ~(0xffu << shift)) | (std::uint32_t(value) << shift));
}
std::uint32_t RecordEntryRules::saved_character(unsigned player, unsigned index) const {
    return std::uint32_t(session_.players().at(player).records.saved_initials.at(index));
}
void RecordEntryRules::saved_character(unsigned player, unsigned index, std::uint32_t value) {
    session_.players().at(player).records.saved_initials.at(index) = bits(value);
}

bool RecordEntryRules::step(std::uint32_t& edges, std::uint32_t& repeats) {
    const auto player = std::uint32_t(rules_.record_entry_player);
    if (player >= 4)
        throw std::out_of_range("Invalid initials player");
    if (!session_.initials_initialized) {
        edges = repeats = 0;
        rules_.initials_cursor = 0;
        for (unsigned i = 0; i < characters; ++i)
            character(i, std::uint8_t(saved_character(player, i)));
        session_.initials_initialized = 1;
    }
    if (effects_)
        effects_->record_page(RecordPage::initials, 2, 1);
    auto selected = rules_.initials_cursor;
    if (selected < 0 || selected > int(characters))
        throw std::out_of_range("Invalid initials cursor");
    if (selected != int(characters)) {
        if (repeats & 2) {
            sound({177, 0, 96, 17});
            const auto value = std::uint8_t(character(unsigned(selected)) + 1);
            character(unsigned(selected), value > last_character ? first_character : value);
        }
        if (repeats & 1) {
            sound({177, 0, 96, 17});
            const auto value = std::uint8_t(character(unsigned(selected)) - 1);
            character(unsigned(selected), value < first_character ? last_character : value);
        }
        if (edges & input_action::confirm) {
            if (character(unsigned(selected)) == last_character) {
                character(unsigned(selected), first_character);
                if (selected > 0)
                    sound({178, 0, 32, 18});
                selected = std::max(0, selected - 1);
            } else {
                ++selected;
                sound({176, 0, 0, 18});
            }
        }
        if (edges & input_action::continue_game) {
            // Confirm+back can reach byte three in this original same-frame order.
            character(unsigned(selected), first_character);
            if (selected > 0)
                sound({178, 0, 32, 18});
            selected = std::max(0, selected - 1);
        }
        rules_.initials_cursor = selected;
        rules_.initials_confirmation_timer = rule_time::second;
        replay_delay_ = rule_time::half_second;
    }
    if (selected == int(characters)) {
        for (unsigned i = 0; i < characters; ++i)
            saved_character(player, i, character(i));
        if (character(0) == 'P' && character(1) == 'R' && character(2) == 'O')
            simulation_.double_steps = 1;
        if (!replay_.awards) {
            const auto remaining = std::max(0, sub(rules_.initials_confirmation_timer, rules_.elapsed));
            rules_.initials_confirmation_timer = remaining;
            if (!remaining)
                return true;
        }
    }
    return false;
}
} // namespace web::rules
