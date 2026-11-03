#include "lamp_presentation.hpp"
#include "../physics/integer.hpp"
#include <stdexcept>

namespace web {
using namespace physics::integer;
std::vector<LampPresentation::Draw> LampPresentation::step(std::span<const std::int32_t, lamp_count> requests, std::span<const std::int32_t, lamp_count> costs, Parameters parameters) {
    constexpr std::int32_t small_budget = 0x92e, medium_budget = 0xe4c, large_budget = 6000;
    const auto resolution = parameters.variant & 0x30;
    state_.budget = resolution == 0x10 ? small_budget : resolution == 0x20 ? medium_budget : resolution == 0x30 ? large_budget : 0;
    state_.budget = mul(state_.budget, parameters.repetitions);
    if (parameters.animation)
        state_.budget = mul(state_.budget, 2);
    if (parameters.variant & 0x40)
        state_.budget = mul(state_.budget, 2);
    if (!parameters.flipping)
        state_.budget >>= 1;
    if (parameters.page == 0) {
        state_.pending_count = 0;
        std::int32_t used = 0;
        // Prioritize direct transitions between nonnegative outputs.
        for (unsigned lamp = 0; lamp < lamp_count; ++lamp) {
            if (state_.displayed_requests[lamp] != requests[lamp] && requests[lamp] >= 0 && state_.displayed_requests[lamp] >= 0) {
                const auto index = unsigned(state_.pending_count++);
                state_.pending_lamps[index] = std::int32_t(lamp);
                state_.pending_values[index] = requests[lamp];
                used = add(used, costs[lamp]);
            }
            if (!state_.force_refresh && used >= state_.budget)
                break;
        }
        if (!state_.force_refresh && used < state_.budget) {
            // Under budget the original replaces that queue, rather than
            // appending to it. A nonnegative transition first displays HALF.
            state_.pending_count = 0;
            auto remaining = state_.budget;
            for (unsigned lamp = 0; lamp < lamp_count; ++lamp) {
                const auto previous = state_.displayed_requests[lamp], requested = requests[lamp];
                if (previous != requested) {
                    const auto index = unsigned(state_.pending_count++);
                    state_.pending_lamps[index] = std::int32_t(lamp);
                    state_.pending_values[index] = previous < 0 || requested < 0 ? requested : -1;
                    remaining = sub(remaining, costs[lamp]);
                }
                if (remaining < 1)
                    break;
            }
        }
        state_.force_refresh = 0;
    }
    if (state_.pending_count < 0 || state_.pending_count > std::int32_t(lamp_count))
        throw std::out_of_range("Invalid original lamp presentation queue length");
    std::vector<Draw> draws;
    draws.reserve(unsigned(state_.pending_count));
    for (auto index = state_.pending_count; index > 0; --index) {
        const auto offset = unsigned(index - 1);
        const auto lamp = state_.pending_lamps[offset];
        auto& value = state_.pending_values[offset];
        if (lamp < 0 || lamp >= std::int32_t(lamp_count))
            throw std::out_of_range("Invalid original displayed lamp");
        if (parameters.page == 0) {
            state_.displayed_requests[unsigned(lamp)] = value;
            if (value > 1)
                value = 1;
            if (value < 0)
                value = 2;
        }
        if (value < 0 || value > 2)
            throw std::out_of_range("Invalid original lamp sprite bank");
        draws.push_back({unsigned(lamp), unsigned(value)});
    }
    return draws;
}
} // namespace web
