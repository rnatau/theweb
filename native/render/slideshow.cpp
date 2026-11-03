#include "slideshow.hpp"
#include "../input.hpp"
#include <stdexcept>

namespace web {
Slideshow::Slideshow(unsigned rate) :
    duration_(rate * seconds_per_image) {
    if (rate < 10 || rate > 1000)
        throw std::out_of_range("Unsupported slideshow cadence");
}
std::optional<unsigned> Slideshow::step(std::uint32_t edges) {
    std::optional<unsigned> image;
    if (exited_)
        return image;
    if (!waiting_) {
        if (shown_ != pending_) {
            shown_ = pending_;
            image = shown_;
        }
        if (next_ < image_count && next_ != pending_)
            pending_ = next_;
        waiting_ = true;
        remaining_ = int(duration_);
    } else if (!--remaining_) {
        if (automatic_) {
            next_ = backwards_ ? shown_ - 2 : shown_ + 2;
            complete_ = shown_ == image_count - 1;
            waiting_ = false;
        } else
            remaining_ = int(duration_);
    } else {
        next_ = (next_ + image_count) % image_count;
        if (edges & input_action::menu_navigation) {
            backwards_ = (edges & input_action::left_flipper) != 0;
            pending_ = ((backwards_ ? shown_ - 1 : shown_ + 1) + image_count) % image_count;
            next_ = ((backwards_ ? shown_ - 2 : shown_ + 2) + image_count) % image_count;
            automatic_ = false;
            waiting_ = false;
        }
    }
    exited_ = complete_ || (edges & input_action::escape);
    return image;
}
std::array<std::uint32_t, 8> Slideshow::state() const {
    return {shown_, pending_, next_, std::uint32_t(remaining_), waiting_, automatic_, backwards_, complete_};
}
} // namespace web
