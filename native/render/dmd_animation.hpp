#pragma once
#include "dot_matrix.hpp"
#include "../sound.hpp"

namespace web {
// Original 0x421bf8 / 0x421d60. DMS offsets stay file-relative, with no pointers
// into mutable buffers and no dependency on the original executable at runtime.
class DmdAnimationPlayer {
public:
    enum Flag : std::uint32_t {
        replace = 1,
        mirror = 2,
        hold = 4
    };
    struct Request {
        std::uint32_t offset = 0, id = 0, repeats = 0, flags = 0;
    };
    struct State {
        std::uint32_t active = 0, flags = 0, repeats = 0, delay = 0, cursor = 0;
        std::int32_t frames = 0, started = 0;
        std::array<Request, 2> queue{};
    };
    explicit DmdAnimationPlayer(const std::filesystem::path& assets);
    void request(unsigned animation, std::uint32_t flags = 0, std::uint32_t repetitions = 1);
    void stop();
    // Remove a presentation family's active and queued requests, retaining others.
    void discard(unsigned first, unsigned last);
    void step();
    // Original 0x40e474 consumes start/frame markers at the rules cadence.
    void sound_step(SoundSink* sink) {
        sound_step(state_, sink);
    }
    static void sound_step(State&, SoundSink*);
    bool active() const {
        return state_.active != 0;
    }
    const State& state() const {
        return state_;
    }
    const DotMatrixFrame& frame() const {
        return display_;
    }
    const DotMatrixFrame& decoded() const {
        return decoded_;
    }
    unsigned count() const {
        return unsigned(bank_.size());
    }

private:
    Container bank_;
    State state_;
    DotMatrixFrame decoded_, display_;
    std::uint32_t start(unsigned animation) const;
    int priority(unsigned animation) const;
    std::uint8_t byte();
    void decode_frame();
    void publish();
};
} // namespace web
