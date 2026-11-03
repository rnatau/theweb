#include "dmd_animation.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web {
namespace {
// Encoded byte groups are traversed along rows/columns in 14 shapes. Modes
// 8..15 visit two bit planes; modes 2..7 store four two-bit pixels per group.
class FrameTraversal {
public:
    explicit FrameTraversal(unsigned mode, DotMatrixFrame& frame) :
        mode_(mode),
        frame_(frame) {
        if (mode > 15)
            throw std::runtime_error("Unknown original DMD frame type");
    }
    int position() const {
        return position_;
    }
    void advance() {
        if (mode_ < 2)
            return;
        const auto shape = dimensions();
        const bool columns_first = (mode_ & 1) != 0;
        const auto columns = DotMatrixFrame::width / shape[0];
        const auto rows = DotMatrixFrame::height / shape[1];
        const auto groups = columns * rows;
        ++group_;
        const auto local = group_ % groups;
        plane_ = unsigned(group_ / groups) & 1;
        const auto x = (columns_first ? local / rows : local % columns) * shape[0];
        const auto y = (columns_first ? local % rows : local / columns) * shape[1];
        // Non-planar final pointers advance past the image in the original.
        position_ = group_ >= groups && mode_ < 8 ? DotMatrixFrame::pixel_count : y * DotMatrixFrame::width + x;
    }
    unsigned read(int position) const {
        const auto shape = dimensions();
        const unsigned bits = mode_ < 8 ? 2 : 1;
        unsigned packed = 0;
        for (int row = 0; row < shape[1]; ++row)
            for (int column = 0; column < shape[0]; ++column)
                packed = (packed << bits) | ((pixel(position + row * DotMatrixFrame::width + column) >> (mode_ < 8 ? 0 : plane_)) & ((1u << bits) - 1));
        return packed;
    }
    void write(unsigned packed) {
        if (mode_ < 2)
            return; // Original 0x421544 has no writes for type zero.
        const auto shape = dimensions();
        const unsigned bits = mode_ < 8 ? 2 : 1;
        const auto shift = mode_ < 8 ? 0 : plane_;
        const auto mask = ((1u << bits) - 1) << shift;
        unsigned remaining = 8;
        for (int row = 0; row < shape[1]; ++row) {
            for (int column = 0; column < shape[0]; ++column) {
                remaining -= bits;
                auto& destination = pixel(position_ + row * DotMatrixFrame::width + column);
                destination = std::uint8_t((destination & ~mask) | (((packed >> remaining) << shift) & mask));
            }
        }
    }
    int previous_position() const {
        constexpr std::array<int, 16> distance = {0, 0, 128, 4, 512, 1, 256, 2, 128, 8, 256, 4, 512, 2, 1024, 1};
        return position_ - distance[mode_];
    }

private:
    unsigned mode_, plane_ = 0;
    int group_ = 0, position_ = 0;
    DotMatrixFrame& frame_;
    std::array<int, 2> dimensions() const {
        if (mode_ < 4)
            return {4, 1};
        if (mode_ < 6)
            return {1, 4};
        if (mode_ < 8)
            return {2, 2};
        if (mode_ < 10)
            return {8, 1};
        if (mode_ < 12)
            return {4, 2};
        if (mode_ < 14)
            return {2, 4};
        return {1, 8};
    }
    std::uint8_t& pixel(int position) const {
        if (position < 0 || std::size_t(position) >= frame_.pixels.size())
            throw std::runtime_error("DMD compressed frame reads outside image: mode " + std::to_string(mode_) + ", position " + std::to_string(position) + ", group " + std::to_string(group_));
        return frame_.pixels[std::size_t(position)];
    }
};
} // namespace
DmdAnimationPlayer::DmdAnimationPlayer(const std::filesystem::path& assets) :
    bank_(Container::load(assets / "MATRIX" / "ANIMS.DMS")) {}
std::uint32_t DmdAnimationPlayer::start(unsigned animation) const {
    if (!animation || animation > bank_.size() || bank_.offsets[animation - 1] + 5 > bank_.bytes.size())
        throw std::out_of_range("Invalid original DMD animation");
    return bank_.offsets[animation - 1] + 4;
}
int DmdAnimationPlayer::priority(unsigned animation) const {
    return std::int16_t(u16(bank_.bytes, start(animation) - 2));
}
void DmdAnimationPlayer::request(unsigned animation, std::uint32_t flags, std::uint32_t repetitions) {
    const Request request{start(animation) - 4, animation, repetitions - 1, flags};
    if (!(flags & replace) && active()) {
        auto& first = state_.queue[0];
        auto& second = state_.queue[1];
        if (!first.id || priority(animation) >= priority(first.id)) {
            second = first;
            first = request;
        } else if (!second.id || priority(animation) >= priority(second.id)) {
            second = request;
        }
        return;
    }
    state_.active = animation;
    state_.flags = flags;
    state_.repeats = request.repeats;
    state_.delay = state_.cursor = 0;
    state_.frames = 0;
    state_.queue[0].id = state_.queue[1].id = 0;
}
void DmdAnimationPlayer::stop() {
    state_.active = state_.queue[0].id = state_.queue[1].id = 0;
}
void DmdAnimationPlayer::discard(unsigned first, unsigned last) {
    const auto matches = [=](unsigned id) { return id >= first && id <= last; };
    std::array<Request, 2> retained{};
    unsigned count = 0;
    for (const auto& request : state_.queue)
        if (request.id && !matches(request.id))
            retained[count++] = request;
    state_.queue = retained;
    if (!state_.active || !matches(state_.active))
        return;
    state_.active = state_.flags = state_.repeats = state_.delay = state_.cursor = 0;
    state_.frames = state_.started = 0;
    if (count) {
        state_.active = retained[0].id;
        state_.flags = retained[0].flags;
        state_.repeats = retained[0].repeats;
        state_.queue[0] = retained[1];
        state_.queue[1] = {};
    }
}
void DmdAnimationPlayer::sound_step(State& state, SoundSink* sink) {
    const auto sound = [&](int id, int attenuation, int group, int pan = 0) {
        if (sink)
            sink->sound({id, pan, attenuation, group});
    };
    if (state.started) {
        switch (state.active) {
            case 23:
                state.started = 0;
                sound(149, 0, 1, -64);
                break;
            case 59:
                state.started = 0;
                sound(176, 64, 17);
                break;
            case 98:
                sound(170, 0, 2);
                state.started = 0;
                break;
            case 125:
                if (state.started == 1) {
                    state.started = 0;
                    sound(92, 0, 1, -64);
                }
                break;
        }
        return;
    }
    int trigger = -1;
    switch (state.active) {
        case 20:
            trigger = 8;
            break;
        case 24:
        case 62:
            trigger = 16;
            break;
        case 26:
            trigger = 24;
            break;
        case 28:
            trigger = 30;
            break;
        case 42:
            trigger = 6;
            break;
        case 73:
            trigger = 52;
            break;
        case 51:
        case 60:
        case 74:
        case 75:
        case 76:
        case 77:
        case 78:
        case 79:
        case 80:
        case 81:
        case 82:
            trigger = 12;
            break;
    }
    if (state.active == 26 && state.frames == 12)
        sound(169, 0, 17);
    if (trigger < 0 || state.frames != trigger)
        return;
    state.frames = -1;
    if (state.active == 82) {
        sound(77, 0, 1);
        return;
    }
    if (state.active == 26) {
        sound(169, 0, 17);
        sound(168, 32, 2);
        return;
    }
    const int sample = state.active == 20 || state.active == 51 ? 169 : state.active == 73 ? 168 : 170;
    sound(sample, state.active == 62 ? 64 : state.active == 73 ? 32 : 0, 2);
    if (state.active == 24 || state.active == 28 || (state.active >= 73 && state.active <= 81))
        sound(169, 0, 17);
    if (state.active == 73)
        sound(169, 0, 18);
}
std::uint8_t DmdAnimationPlayer::byte() {
    // Entry 124 aliases 125 in the original offset table (zero-length block).
    const auto end = state_.active ? *std::upper_bound(bank_.offsets.begin(), bank_.offsets.end(), bank_.offsets.at(state_.active - 1)) : bank_.bytes.size();
    if (state_.cursor >= end)
        throw std::runtime_error("Truncated original DMD stream");
    return bank_.bytes.at(state_.cursor++);
}
void DmdAnimationPlayer::decode_frame() {
    const auto mode = byte();
    if (mode == 1) {
        for (unsigned group = 0; group < DotMatrixFrame::pixel_count / 4; ++group) {
            const auto packed = byte();
            for (unsigned pixel = 0; pixel < 4; ++pixel)
                decoded_.pixels[group * 4 + pixel] = (packed >> (6 - 2 * pixel)) & 3;
        }
    } else {
        FrameTraversal traversal(mode, decoded_);
        const std::array<std::uint8_t, 3> markers = {byte(), byte(), byte()};
        unsigned remaining = DotMatrixFrame::pixel_count / 4, last = 0;
        while (remaining) {
            const auto packed = byte();
            if (packed == markers[0]) {
                const unsigned skipped = unsigned(byte()) + 2;
                if (skipped >= remaining)
                    throw std::runtime_error("DMD skip exceeds frame");
                remaining -= skipped;
                for (unsigned i = 0; i < skipped; ++i)
                    traversal.advance();
                last = traversal.read(traversal.position());
                traversal.advance();
                --remaining;
            } else {
                const bool previous = packed == markers[1], repeat_last = packed == markers[2];
                const unsigned count = previous || repeat_last ? unsigned(byte()) + 3 : 1;
                if (count > remaining)
                    throw std::runtime_error("DMD run exceeds frame");
                for (unsigned i = 0; i < count; ++i) {
                    if (previous)
                        last = traversal.read(traversal.previous_position());
                    else if (!repeat_last)
                        last = packed;
                    traversal.write(last);
                    traversal.advance();
                }
                remaining -= count;
            }
        }
    }
    if (state_.frames >= 0)
        state_.frames = physics::integer::add(state_.frames, 1);
}
void DmdAnimationPlayer::publish() {
    if (state_.flags & hold)
        state_.delay = 255;
    display_ = decoded_;
    if (state_.flags & mirror)
        for (int row = 0; row < DotMatrixFrame::height; ++row)
            std::reverse(display_.pixels.begin() + row * DotMatrixFrame::width, display_.pixels.begin() + (row + 1) * DotMatrixFrame::width);
}
void DmdAnimationPlayer::step() {
    state_.started = 0;
    if (!active())
        return;
    if (!state_.cursor) {
        state_.cursor = start(state_.active);
        state_.started = 1;
        state_.frames = 0;
    }
    if (state_.delay) {
        if (state_.delay != 255)
            --state_.delay;
        if (state_.delay) {
            publish();
            return;
        }
    }
    // A bounded command budget rejects malformed zero-frame loops; valid media
    // reaches a frame/wait without depending on the original executable's memory.
    constexpr unsigned command_budget = 4096;
    for (unsigned command = 0; command < command_budget; ++command) {
        const auto cursor = state_.cursor;
        const auto opcode = byte();
        if (opcode == 0x81) {
            state_.delay = byte();
            publish();
            return;
        }
        if (opcode == 0x82 || opcode == 0x83) {
            const auto repeats = byte();
            if (!state_.repeats)
                state_.repeats = repeats;
            else if (state_.repeats != 255)
                --state_.repeats;
            if (opcode == 0x82) {
                if (state_.repeats) {
                    state_.cursor = start(state_.active);
                    state_.frames = 0;
                    state_.started = -1;
                }
            } else {
                const auto low = byte();
                const unsigned distance = low + unsigned(byte()) * 256;
                if (state_.repeats) {
                    if (distance > cursor + 4 - start(state_.active))
                        throw std::runtime_error("DMD loop jumps outside animation");
                    state_.cursor -= distance;
                }
            }
        } else if (opcode == 255) {
            if (state_.repeats) {
                if (state_.repeats != 255)
                    --state_.repeats;
                state_.cursor = start(state_.active);
                state_.started = -1;
            } else if (state_.queue[0].id) {
                const auto next = state_.queue[0];
                state_.active = next.id;
                state_.flags = next.flags;
                state_.cursor = next.offset + 4;
                state_.frames = 0;
                state_.started = 1;
                state_.queue[0] = state_.queue[1];
                state_.queue[1].id = 0;
                // Original does not copy the queued repeat count here.
            } else {
                state_.active = 0;
                state_.cursor = cursor;
                publish();
                return;
            }
        } else {
            state_.cursor = cursor;
            decode_frame();
            publish();
            return;
        }
    }
    throw std::runtime_error("Original DMD stream has no terminating frame or wait");
}
} // namespace web
