#pragma once
#include "game_session.hpp"
#include "simulation_clock.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>

namespace web {
// Fixed 60 Hz game ticks with resumable physics. Elapsed simulation time is
// independent of render calls; each call executes only whole due microsteps.
class IncrementalSession {
public:
    struct Work {
        unsigned ticks = 0, microsteps = 0;
    };
    void reset(std::uint64_t now_ns, const GameSession& game, std::uint32_t held = 0) {
        last_ns_ = now_ns;
        anchored_ = true;
        if (!game.remaining_microsteps()) {
            count_ = game.physics_microsteps();
            completed_ = 0;
        } else
            completed_ = count_ - game.remaining_microsteps();
        units_ = count_ ? std::uint64_t(completed_) * second / count_ : 0;
        rule_input_ = {held, 0, 0};
    }
    Work advance(std::uint64_t now_ns, GameSession& game, InputTimeline& input, const std::function<void()>& before_tick, const std::function<bool()>& after_tick) {
        if (!anchored_)
            reset(now_ns, game);
        if (now_ns < last_ns_)
            throw std::invalid_argument("Incremental clock moved backwards");
        const auto elapsed = now_ns - last_ns_;
        last_ns_ = now_ns;
        units_ += std::min(elapsed, second / 4) * hz;
        if (elapsed > second / 4)
            rule_input_.pressed = rule_input_.released = 0;
        Work result;
        for (;;) {
            if (!game.remaining_microsteps()) {
                // Discarded gaps carry final held input, never obsolete taps.
                const auto start = boundary(now_ns, 0, game.physics_microsteps());
                rule_input_.actions = input.sample(start, start).actions;
                before_tick();
                TimedInputFrame rules;
                rules.rule_actions = rule_input_.actions | rule_input_.pressed;
                rules.pressed = rule_input_.pressed;
                rules.released = rule_input_.released;
                rules.microsteps.push_back({rule_input_.actions, 0});
                rule_input_.pressed = rule_input_.released = 0;
                count_ = game.begin_tick(rules); // Rules may select 17 or 34.
                completed_ = 0;
                current_active_.fill(0);
                if (!count_ || count_ > 2100)
                    throw std::logic_error("Invalid incremental physics count");
            }
            if (!due(completed_ + 1, count_))
                break;
            const auto sample = input.sample(boundary(now_ns, completed_, count_), boundary(now_ns, completed_ + 1, count_));
            rule_input_.actions = sample.actions;
            rule_input_.pressed |= sample.pressed;
            rule_input_.released |= sample.released;
            for (unsigned bit = 0; bit < current_active_.size(); ++bit)
                current_active_[bit] += bool(sample.actions & (1u << bit));
            const bool finished = game.step_microstep({sample.actions, sample.pressed});
            ++completed_;
            ++result.microsteps;
            ++microsteps_;
            if (finished) {
                units_ -= second;
                completed_ = 0;
                completed_active_ = current_active_;
                ++result.ticks;
                ++ticks_;
                if (!after_tick())
                    break;
            }
        }
        return result;
    }
    const std::array<unsigned, 32>& active_microsteps() const {
        return completed_active_;
    }
    std::uint64_t microsteps() const {
        return microsteps_;
    }
    std::uint64_t ticks() const {
        return ticks_;
    }

private:
    static constexpr std::uint64_t second = 1000000000, hz = 60;
    std::uint64_t last_ns_ = 0, units_ = 0, microsteps_ = 0, ticks_ = 0;
    unsigned count_ = 17, completed_ = 0;
    bool anchored_ = false;
    InputTransitions rule_input_;
    std::array<unsigned, 32> current_active_{}, completed_active_{};
    bool due(unsigned index, unsigned count) const {
        return units_ * count >= std::uint64_t(index) * second;
    }
    std::uint64_t boundary(std::uint64_t now_ns, unsigned index, unsigned count) const {
        // Rational endpoints are rounded only at the nanosecond input boundary.
        return now_ns - (units_ * count - std::uint64_t(index) * second) / (hz * count);
    }
};

// Presentation deadlines have their own fractional accumulator. Missed images
// are skipped while the simulation still catches up through its fixed steps.
class PresentationClock {
public:
    void rate(unsigned value) {
        if (!value || value > 1000)
            throw std::invalid_argument("Unsupported presentation rate");
        if (value != rate_) {
            rate_ = value;
            reset();
        }
    }
    unsigned rate() const {
        return rate_;
    }
    void reset() {
        units_ = 0;
    }
    bool advance(std::uint64_t elapsed_ns) {
        units_ += std::min<std::uint64_t>(elapsed_ns, 250000000) * rate_;
        const bool ready = units_ >= 1000000000;
        units_ %= 1000000000;
        return ready;
    }
    std::uint64_t until_next_frame_ns() const {
        return (1000000000 - units_ + rate_ - 1) / rate_;
    }

private:
    unsigned rate_ = 60;
    std::uint64_t units_ = 0;
};
} // namespace web
