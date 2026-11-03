#include "video_mode.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web::rules {
namespace F = web::rules::turn_field;
namespace S = web::rules::session_field;

using namespace physics::integer;
namespace {
constexpr int spawn_initial = 12, extra_ball_interval = 64, bonus_interval = 12;
constexpr unsigned mine_spawn_mask = 3, extra_ball_chance_mask = 7; // 3/4 mines; 1/8 extra-ball chance after the first award.
constexpr int minimum_spawn_period = 3, spawn_period_growth = 6, countdown_scale = 360;
constexpr int initial_countdown = 312, end_countdown = -60, result_countdown = -44, last_spawn = -36;
// Countdown/spawn intervals count DMD updates, not rule timer units.
using namespace video_geometry;
constexpr unsigned sphere_animation = 125;
constexpr unsigned video_music_track = 16;
} // namespace
Player& VideoModeRules::display_player() {
    return session_.players().at(std::uint32_t(rules_.player_turn));
}
void VideoModeRules::page(VideoPage kind, unsigned ticks) {
    if (effects_)
        effects_->video_page(kind, ticks, -1);
}
void VideoModeRules::score(int factor) {
    scoring_.hundred_thousands(std::uint32_t(factor));
}
void VideoModeRules::initialize() {
    state_.countdown = initial_countdown;
    state_.misses = std::max(1, sub(3, display_player().video.extra_ball_cycles));
    for (auto& object : state_.objects)
        object.type = video_object::empty;
    state_.pending_shot = state_.queued_shot = 0;
    state_.spawn_period = state_.spawn_timer = state_.until_bonus = spawn_initial;
    state_.until_extra_ball = extra_ball_interval;
    LampRules{}.begin_animation(rules_, session_, 3);
    state_.perfect = 1;
    state_.total = 0;
}
void VideoModeRules::start() {
    auto& p = session_.players().at(std::uint32_t(rules_.current_player));
    p.video.rounds = add(p.video.rounds, 1);
    scoring_.stop_music();
    scoring_.sound({120, -64, 0, 1});
    if (effects_) {
        effects_->video_stop_animations();
        effects_->video_page(VideoPage::instructions, 36, 1);
    }
    rules_.video_active = 1;
    initialize();
}
void VideoModeRules::step_contacts(bool allow_start) {
    auto& p = session_.players().at(std::uint32_t(rules_.current_player));
    if (allow_start && rules_.switch_edge(TableSwitch::sinkhole_b) && !rules_.mission_time && rules_.single_ball_award_mode != mode_state::active && p.video.ready == 1 && !rules_.drain_override) {
        rules_.video_lock_pending = 1;
        p.video.ready = 0;
        start();
    }
    if (!rules_.video_active)
        rules_.video_lock_pending = 0;
}
void VideoModeRules::input(std::uint32_t& edges, std::uint32_t held) {
    rules_.returns_paused = 1;
    auto intro = session_.dmd_animation;
    if ((held & input_action::both_flippers) == input_action::both_flippers && (edges & input_action::both_flippers) && intro) {
        intro = 1;
        session_.dmd_animation = intro;
        edges = 0;
        state_.pending_shot = state_.queued_shot = 0;
    }
    if (intro == 1) {
        session_.dmd_animation = 0;
        session_.dmd_frame = 0;
        session_.dmd_aux = -1;
        session_.dmd_status = dmd_status::video;
        if (session_.music_track != int(video_music_track)) {
            rules_.music_fade_timer = 0;
            scoring_.stop_sounds();
            scoring_.music_track(video_music_track);
        }
        if (effects_)
            effects_->video_clear_page();
    }
    simulation_.mechanisms.enabled = 0;
    const auto queue = [&](int shot) {
        if (!state_.pending_shot)
            state_.pending_shot = shot;
        else if (!state_.queued_shot)
            state_.queued_shot = shot;
    };
    if (edges & input_action::right_flipper)
        queue(2);
    if (edges & input_action::left_flipper)
        queue(1);
}
void VideoModeRules::resolve(int type) {
    scoring_.sound({169, 0, 0, 18});
    switch (type) {
        case video_object::mine: {
            const auto factor = display_player().video.rounds;
            state_.total = add(state_.total, factor);
            score(factor);
            break;
        }
        case video_object::extra_ball:
        case video_object::fixed_award:
            state_.perfect = 0;
            if (type == video_object::extra_ball)
                scoring_.sound({71, 0, 0, 1});
            break;
        case video_object::round_award:
            state_.misses = sub(state_.misses, 1);
            state_.damaged = 1;
            break;
    }
}
void VideoModeRules::advance_object(unsigned index) {
    if (index >= state_.objects.size())
        throw std::out_of_range("Original video object slot");
    auto& object = state_.objects[index];
    if (object.type > video_object::fixed_award) {
        object.type = add(object.type, 1);
        if (object.type == video_object::animation_end)
            object.type = video_object::empty;
        return;
    }
    object.depth = add(object.depth, 1);
    if (object.depth != video_geometry::depth_steps)
        return;
    switch (object.type) {
        case video_object::mine:
            state_.misses = sub(state_.misses, 1);
            state_.damaged = 1;
            break;
        case video_object::extra_ball:
            extra_balls_.award_without_animation();
            break;
        case video_object::round_award: {
            scoring_.sound({37, 64, 0, 1});
            const auto factor = mul(display_player().video.rounds, 2);
            state_.total = add(state_.total, factor);
            score(factor);
            break;
        }
        case video_object::fixed_award:
            scoring_.sound({4, 64, 64, 1});
            state_.total = add(state_.total, 5);
            score(5);
            break;
    }
    object.type = video_object::empty;
}
void VideoModeRules::spawn(int type, int lane) {
    VideoObject* free = nullptr;
    for (auto& object : state_.objects)
        if (!object.type)
            free = &object;
    if (free)
        *free = {type, lane, 0};
}
void VideoModeRules::shoot(bool right) {
    state_.aim = right ? right_aim : left_aim;
    state_.laser = right ? 1 : -1;
    for (int depth = video_geometry::depth_steps - 1; depth > 1; --depth) {
        bool hit = false;
        for (auto& object : state_.objects)
            if (object.type != video_object::empty && object.type < video_object::explosion_first && object.depth == depth &&
                (right ? object.lane >= video_geometry::lanes / 2 : object.lane < video_geometry::lanes / 2)) {
                resolve(object.type);
                object.type = video_object::explosion_first;
                hit = true;
            }
        if (hit)
            break;
    }
}
void VideoModeRules::consume_shot() {
    // The default retains 0x415078's first-word-only dispatch. An injected
    // queue policy promotes the second shot for the next DMD update.
    if (queue_advance_ && state_.pending_shot == 0)
        queue_advance_(state_);
    if (state_.pending_shot == 1 || state_.pending_shot == 2) {
        const bool right = state_.pending_shot == 2;
        state_.pending_shot = 0;
        if (queue_advance_)
            queue_advance_(state_);
        shoot(right);
    }
}
void VideoModeRules::finish() {
    scoring_.stop_music();
    rules_.video_active = 0;
    rules_.returns_paused = 0;
    session_.dmd_status = dmd_status::table;
    if (state_.misses >= 0) {
        auto& p = session_.players().at(std::uint32_t(rules_.current_player));
        p.showdown.spheres = add(p.showdown.spheres, 1);
        if (effects_)
            effects_->video_animation(sphere_animation, 0);
        score(50);
    }
    simulation_.mechanisms.enabled = 1;
    SessionRules{}.clear_lamp_animation(rules_, session_);
    rules_.video_lock_pending = add(rules_.video_lock_pending, 1);
}
void VideoModeRules::show_result() {
    if (!state_.perfect)
        page(VideoPage::result, 24);
    else {
        auto& p = display_player();
        p.video.perfect_rounds = add(p.video.perfect_rounds, 1);
        page(VideoPage::perfect, 36);
        state_.total = mul(p.video.perfect_rounds, 250);
        score(state_.total);
    }
    scoring_.sound({state_.perfect ? 39 : 38, 64, 0, 1});
    state_.countdown = end_countdown;
}
void VideoModeRules::update_spawning() {
    // Keep the multiply before division: the original wraps its numerator.
    state_.spawn_period = std::max(minimum_spawn_period, add(mul(state_.countdown, spawn_period_growth) / countdown_scale, minimum_spawn_period));
    if (state_.countdown > last_spawn) {
        state_.spawn_timer = sub(state_.spawn_timer, 1);
        if (!state_.spawn_timer) {
            state_.spawn_timer = state_.spawn_period;
            int type = (simulation_.random.next() & mine_spawn_mask) ? video_object::mine : video_object::fixed_award;
            state_.until_extra_ball = sub(state_.until_extra_ball, 1);
            if (!state_.until_extra_ball) {
                auto& p = display_player();
                p.video.extra_ball_cycles = add(p.video.extra_ball_cycles, 1);
                if (!p.awards.extra_ball_awards || !(simulation_.random.next() & extra_ball_chance_mask))
                    type = video_object::extra_ball;
            } else {
                state_.until_bonus = sub(state_.until_bonus, 1);
                if (!state_.until_bonus) {
                    state_.until_bonus = bonus_interval;
                    type = video_object::round_award;
                }
            }
            auto lane = int(simulation_.random.next() & (video_geometry::lanes - 1));
            if (lane == state_.last_lane)
                lane = add(state_.last_lane, video_geometry::lanes / 2) & (video_geometry::lanes - 1);
            state_.last_lane = lane;
            spawn(type, lane);
        }
    }
}
void VideoModeRules::report_damage_and_failure() {
    if (state_.damaged) {
        state_.perfect = 0;
        scoring_.stop_sounds(-1, -1, 1);
        scoring_.sound({172, 0, 0, 1});
        scoring_.sound({169, 0, 0, 18});
    }
    if (state_.misses < 0) {
        scoring_.stop_music();
        scoring_.stop_sounds();
        const auto voice = (rules_.frame_phase() >> 3) & 3;
        scoring_.sound({voice < 2 ? 79 : voice == 2 ? 64 : 81, 0, 0, 1});
        scoring_.sound({172, 0, 0, 17});
        scoring_.sound({169, 0, 0, 18});
        page(VideoPage::result, 24);
    }
}
void VideoModeRules::step(std::uint32_t held) {
    state_.countdown = sub(state_.countdown, 1);
    if (state_.countdown < end_countdown || state_.misses < 0) {
        finish();
        return;
    }
    if (state_.countdown == result_countdown) {
        show_result();
    }
    state_.damaged = 0;
    state_.aim = centre_aim;
    state_.laser = 0;
    consume_shot();
    if (state_.aim == centre_aim) {
        if (held & input_action::left_flipper)
            state_.aim = left_aim;
        else if (held & input_action::right_flipper)
            state_.aim = right_aim;
    }
    for (unsigned i = 0; i < state_.objects.size(); ++i)
        if (state_.objects[i].type)
            advance_object(i);
    update_spawning();
    // Original 0x415386 -> 0x414a80 emits this while drawing the laser,
    // after object updates and before damage/failed-game requests.
    if (state_.laser == 1 || state_.laser == -1)
        scoring_.sound({179, state_.laser == 1 ? 64 : -64, 80, 17});
    report_damage_and_failure();
}
} // namespace web::rules
