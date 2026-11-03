#pragma once
#include "../web.hpp"
#include <optional>
#include <stdexcept>

namespace web {
// MENU 0x405190. Logical palette changes only after interpolation completes;
// an input interruption leaves the last emitted device palette in place.
class PaletteTransition {
public:
    using Palette = std::array<std::uint8_t, 768>;
    explicit PaletteTransition(Palette initial = {}) :
        logical_(initial) {}
    void begin(Palette target, std::int32_t duration) {
        if (duration > 32767)
            throw std::out_of_range("Palette duration exceeds original signed word");
        target_ = target;
        duration_ = duration;
        remaining_ = duration;
        active_ = duration > 0 && target != logical_;
    }
    std::optional<Palette> step(bool interrupt = false) {
        if (!active_)
            return {};
        Palette output;
        for (std::size_t i = 0; i < output.size(); ++i)
            output[i] = std::uint8_t((target_[i] * (duration_ - remaining_) + logical_[i] * remaining_) / duration_);
        if (interrupt || remaining_ == 0) {
            active_ = false;
            logical_ = target_;
        } else
            --remaining_;
        return output;
    }
    const Palette& logical() const {
        return logical_;
    }
    bool active() const {
        return active_;
    }
    std::int32_t remaining() const {
        return remaining_;
    }

private:
    Palette logical_, target_{};
    std::int32_t duration_ = 0, remaining_ = 0;
    bool active_ = false;
};
} // namespace web
