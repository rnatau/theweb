#pragma once
#include "../physics/integer.hpp"
#include <cstdint>
#include <string>
namespace web::rules {
struct ScoreValue {
    static constexpr std::uint32_t radix = 100000000;
    static constexpr unsigned points_per_unit = 10;
    std::uint32_t units = 0, rollovers = 0;
    std::uint64_t points() const;
    std::string text() const;
};
class PlayerScore {
public:
    std::int32_t units = 0, rollovers = 0;
    ScoreValue value() const {
        return {std::uint32_t(units), std::uint32_t(rollovers)};
    }
    void add_units(std::uint32_t amount) {
        const std::uint32_t total = std::uint32_t(units) + amount;
        units = physics::integer::bits(total % ScoreValue::radix);
        rollovers = physics::integer::add(rollovers, std::int32_t(total / ScoreValue::radix));
    }
};
} // namespace web::rules
