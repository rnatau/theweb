#pragma once
#include "rules/missions.hpp"
#include "rules/video_mode.hpp"

namespace web {
// Optional behavior supplied by a game's factory. Defaults preserve the
// recovered controllers and their existing safety rejections.
struct GameCorrections {
    physics::PhysicsPolicy physics;
    rules::BallLifecycle::LauncherSelection launcher = nullptr;
    rules::MissionRules::SelectionFallback mission_selection = nullptr;
    rules::VideoModeRules::QueueAdvance video_queue = nullptr;
    void (*physical_sound)(SoundRequest&, const physics::BallState&) = nullptr;
    bool fit_hideout_caption = false;
    bool improved_records = false;
    void (*initialize_mechanisms)(physics::MechanismState&) = nullptr;
};
} // namespace web
