#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <numeric>
#include <stdexcept>

namespace web {
// Original 0x414222..0x414293 selects by the previous projected Y. Each draw
// then refreshes that slot's projection; ties retain ascending slot order.
class BallDrawOrder {
public:
    using Coordinates = std::array<std::int32_t, 6>;
    BallDrawOrder() = default;
    explicit BallDrawOrder(Coordinates previous_y) :
        previous_y_(previous_y) {}
    std::array<unsigned, 6> step(const Coordinates& current_y) {
        for (auto y : previous_y_)
            if (y >= 0x7fff)
                throw std::out_of_range("Original ball drawing selector has no candidate below its sentinel");
        std::array<unsigned, 6> order;
        std::iota(order.begin(), order.end(), 0u);
        std::stable_sort(order.begin(), order.end(), [&](unsigned left, unsigned right) { return previous_y_[left] < previous_y_[right]; });
        previous_y_ = current_y;
        return order;
    }
    const Coordinates& previous_y() const {
        return previous_y_;
    }

private:
    Coordinates previous_y_{};
};
} // namespace web
