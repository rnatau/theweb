#pragma once
#include "lamps.hpp"
#include "../sound.hpp"
#include <functional>
#include <utility>

namespace web::rules {
// Original 0x4083e8 / 0x408490 / 0x408550 / 0x4085dc. Collection and direct
// awards share counters but have different callback ordering and lit-state writes.
class ExtraBallRules {
public:
    ExtraBallRules(TurnState& rules, SharedGameState& session, std::function<void()> animation = {}) :
        rules_(rules),
        session_(session),
        animation_(std::move(animation)) {}
    void collect_lit();
    void collect_diverter();
    void award();
    void award_without_animation();
    void sound_sink(SoundSink* sink) {
        sounds_ = sink;
    }

private:
    enum class Source {
        lit,
        diverter,
        direct,
        unanimated
    };
    TurnState& rules_;
    SharedGameState& session_;
    std::function<void()> animation_;
    SoundSink* sounds_ = nullptr;
    void apply(Source source);
};
} // namespace web::rules
