#pragma once
#include "sound.hpp"
#include <variant>
namespace web {
struct AnimationRequest {
    std::int32_t id, flags;
};
struct RuleDisplayRequest {
    std::int32_t selector, ticks, priority;
};
struct MusicStopRequest {};
struct SoundStopRequest {
    std::int32_t id, channel, group;
};
// The payload determines the kind; impossible kind/argument combinations cannot
// be constructed. Delivery remains synchronous at the original call boundary.
template <class KindValue, class... Payloads> class TypedEffect {
public:
    using Kind = KindValue;
    template <class T>
    explicit TypedEffect(T value) :
        payload_(value) {}
    Kind kind() const {
        return Kind(payload_.index() + 1);
    }
    template <class T> const T& get() const {
        return std::get<T>(payload_);
    }

private:
    std::variant<Payloads...> payload_;
};
} // namespace web
