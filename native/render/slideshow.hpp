#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace web {
// MENU 0x402eb4 control, with file prefetch/display calls as external boundaries.
class Slideshow {
public:
    static constexpr unsigned image_count = 17;
    explicit Slideshow(unsigned frames_per_second = 60);
    std::optional<unsigned> step(std::uint32_t edges);
    bool finished() const {
        return exited_;
    }
    unsigned image() const {
        return shown_;
    }
    bool automatic() const {
        return automatic_;
    }
    int remaining() const {
        return remaining_;
    }
    std::array<std::uint32_t, 8> state() const;

private:
    static constexpr unsigned seconds_per_image = 10;
    unsigned duration_, shown_ = 0xffffffffu, pending_ = 0, next_ = 1;
    int remaining_ = 0;
    bool waiting_ = false, automatic_ = true, backwards_ = false, complete_ = false, exited_ = false;
};
} // namespace web
