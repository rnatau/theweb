#pragma once
#include "extra_balls.hpp"
#include "scoring.hpp"

namespace web::rules {
namespace video_object {
inline constexpr std::int32_t empty = 0, mine = 1, extra_ball = 2, round_award = 3, fixed_award = 4;
inline constexpr std::int32_t explosion_first = 5, explosion_last = 7, animation_end = 8;
} // namespace video_object
namespace video_geometry {
inline constexpr unsigned object_slots = 8;
inline constexpr int lanes = 4, depth_steps = 8;
inline constexpr int centre_aim = 64, left_aim = 32, right_aim = 96;
} // namespace video_geometry
struct VideoObject {
    std::int32_t type = 0, lane = 0, depth = 0;
};
// Original 0x431db0..0x431e4f, independent of the table physics records.
struct VideoModeState {
    std::array<VideoObject, video_geometry::object_slots> objects{};
    std::int32_t aim = 0, spawn_timer = 0, until_extra_ball = 0, reserved_1c = 0;
    std::int32_t misses = 0, reserved_24 = 0, pending_shot = 0, queued_shot = 0;
    std::int32_t until_bonus = 0, laser = 0, countdown = 0, last_lane = 0;
    std::int32_t damaged = 0, spawn_period = 0, total = 0, perfect = 0;
};
static_assert(sizeof(VideoModeState) == 160);
enum class VideoPage : unsigned {
    instructions = 55,
    perfect = 64,
    result = 64
};
class VideoModeEffects {
public:
    virtual ~VideoModeEffects() = default;
    virtual void video_page(VideoPage page, unsigned ticks, int priority) = 0;
    virtual void video_stop_animations() = 0;
    virtual void video_clear_page() = 0;
    virtual void video_animation(unsigned id, std::uint32_t flags) = 0;
};
// Original video-mode rules; input is sampled on simulation frames, while step
// is called once per original DMD update. Rendering is a separate consumer.
class VideoModeRules {
public:
    using QueueAdvance = void (*)(VideoModeState&);
    VideoModeRules(
        TurnState& rules,
        SharedGameState& session,
        ScoringRules& scoring,
        ExtraBallRules& extra_balls,
        VideoModeState& state,
        physics::SimulationState& simulation,
        VideoModeEffects* effects = nullptr,
        QueueAdvance queue_advance = nullptr) :
        rules_(rules),
        session_(session),
        scoring_(scoring),
        extra_balls_(extra_balls),
        state_(state),
        simulation_(simulation),
        effects_(effects),
        queue_advance_(queue_advance) {}
    void start();                                         // 0x406d18
    void step_contacts(bool allow_start = true);          // Video arm of 0x4106ef..0x4107a7; production retains the caller's branch decision.
    void initialize();                                    // 0x414d54
    void input(std::uint32_t& edges, std::uint32_t held); // 0x406d7c / 0x414df4
    void step(std::uint32_t held);                        // 0x4150d4, no wall-clock substitution
    void resolve(int type);                               // 0x414eec
    void advance_object(unsigned index);                  // 0x414c00
    void spawn(int type, int lane);                       // 0x414d04
    void shoot(bool right);                               // 0x414f80 / 0x414ff8
    void consume_shot();                                  // 0x415078; default retains the original queue quirk.
private:
    void show_result();
    void update_spawning();
    void report_damage_and_failure();
    TurnState& rules_;
    SharedGameState& session_;
    ScoringRules& scoring_;
    ExtraBallRules& extra_balls_;
    VideoModeState& state_;
    physics::SimulationState& simulation_;
    VideoModeEffects* effects_;
    QueueAdvance queue_advance_;
    Player& display_player();
    void page(VideoPage page, unsigned ticks);
    void score(int factor);
    void finish();
};
} // namespace web::rules
