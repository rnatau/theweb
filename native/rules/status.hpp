#pragma once
#include "ball_rules.hpp"

namespace web::rules {
class StatusEffects {
public:
    virtual ~StatusEffects() = default;
    virtual void status_page(int selection) = 0;
};
// Original 0x40e038: held-flipper information pages and contact cancellation.
class StatusRules {
public:
    void step(TurnState&, std::uint32_t edges, std::uint32_t held, StatusEffects* = nullptr) const;
};
} // namespace web::rules
