#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace web {
// WGAME 0x4044e0. Requested rules outputs and displayed lamp state are distinct.
// Original raw requests are retained even when their sprite bank is clamped.
class LampPresentation {
public:
    static constexpr unsigned lamp_count = 79;
    struct State {
        std::array<std::int32_t, lamp_count> displayed_requests{};
        std::array<std::int32_t, lamp_count> pending_lamps{};
        std::array<std::int32_t, lamp_count> pending_values{};
        std::int32_t pending_count = 0;
        std::int32_t force_refresh = 0;
        std::int32_t budget = 0;
    };
    struct Parameters {
        unsigned variant;
        std::int32_t repetitions = 1;
        bool animation = false;
        bool flipping = false;
        unsigned page = 0;
    };
    struct Draw {
        unsigned lamp, bank; // Bank 0=off, 1=on, 2=half.
        bool operator==(const Draw&) const = default;
    };
    LampPresentation() = default;
    explicit LampPresentation(State state) :
        state_(state) {}
    std::vector<Draw> step(std::span<const std::int32_t, lamp_count> requests, std::span<const std::int32_t, lamp_count> costs, Parameters parameters);
    const State& state() const {
        return state_;
    }
    void force_refresh() {
        state_.force_refresh = 1;
    }

private:
    State state_;
};
} // namespace web
