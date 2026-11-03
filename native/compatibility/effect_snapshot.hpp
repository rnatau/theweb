#pragma once
#include "../rules/session_rules.hpp"
namespace web::compatibility {
inline std::array<std::int32_t, 4> parameters(SoundRequest s) {
    return {s.id, s.pan, s.attenuation, s.group};
}
inline std::array<std::int32_t, 4> parameters(const physics::PhysicsEvent& e) {
    using K = physics::PhysicsEvent::Kind;
    switch (e.kind()) {
        case K::sound:
            return parameters(e.get<SoundRequest>());
        case K::switch_contact:
            return {e.get<physics::SwitchContact>().switch_id, 0, 0, 0};
        case K::surface_rule:
            return {e.get<physics::SurfaceContact>().transition, 0, 0, 0};
        case K::spinner_crossing:
            return {e.get<physics::SpinnerCrossing>().spinner, 0, 0, 0};
        default:
            return {};
    }
}
inline std::array<std::int32_t, 4> parameters(const rules::BallRuleEffect& e) {
    using K = rules::BallRuleEffect::Kind;
    switch (e.kind()) {
        case K::sound:
            return parameters(e.get<SoundRequest>());
        case K::display: {
            auto r = e.get<AnimationRequest>();
            return {r.id, r.flags, 0, 0};
        }
        case K::station_contact:
            return {std::int32_t(e.slot), 0, 0, 0};
    }
    return {};
}
inline std::array<std::int32_t, 4> parameters(const rules::SessionEffect& e) {
    using K = rules::SessionEffect::Kind;
    switch (e.kind()) {
        case K::sound:
            return parameters(e.get<SoundRequest>());
        case K::rule_display: {
            auto r = e.get<RuleDisplayRequest>();
            return {r.selector, r.ticks, r.priority, 0};
        }
        case K::music_command:
            return {0x808, 0, 0, 0};
        case K::stop_sounds: {
            auto r = e.get<SoundStopRequest>();
            return {r.id, r.channel, r.group, 0};
        }
    }
    return {};
}
} // namespace web::compatibility
