#pragma once
#include <windows.h>
#include "../../host_settings.hpp"
#include "../../render/presentation.hpp"
#include "../../render/dot_matrix.hpp"
#include <algorithm>
#include <optional>
namespace web::development {
// Read-only owned-process observations. Stable message values let desktop tests
// inspect real application lifetimes without changing rules or physics.
enum ProbeMessage : UINT {
    query_game_frames = WM_APP + 1,
    query_game_phase = WM_APP + 2,
    query_presented_frames = WM_APP + 3,
    query_overlay_visible = WM_APP + 4,
    query_high_resolution_timer = WM_APP + 5,
    query_pixel_scale = WM_APP + 6,
    query_audio_buffers = WM_APP + 7,
    query_configuration_word = WM_APP + 8,
    query_actions = WM_APP + 9,
    query_players = WM_APP + 10,
    query_variant = WM_APP + 11,
    query_raster_counter = WM_APP + 12,
    query_dmd_pixels = WM_APP + 13,
    query_music_track = WM_APP + 14,
    query_music_frames = WM_APP + 15,
    query_console_attached = WM_APP + 16,
    query_startup_ticks = WM_APP + 17,
    query_audio_rate = WM_APP + 18,
    query_audio_channels = WM_APP + 19,
    query_audio_format = WM_APP + 20,
    query_nudge_presentations = WM_APP + 21,
    query_exit_display = WM_APP + 22,
    query_fullscreen = WM_APP + 23,
    query_integer_scaling = WM_APP + 24,
    query_nudge_x = WM_APP + 25,
    query_nudge_y = WM_APP + 26,
    query_rejected_starts = WM_APP + 27,
    query_restart_pending = WM_APP + 28,
    query_active_balls = WM_APP + 29,
    query_ball_position = WM_APP + 30,
    query_waiting_launch = WM_APP + 31,
    query_launches = WM_APP + 32,
    query_music_starts = WM_APP + 33,
    query_ball_z = WM_APP + 34,
    query_depth_view = WM_APP + 35,
    query_timed_input_frames = WM_APP + 36,
    query_input_active_microsteps = WM_APP + 37,
    query_gpu_presentation = WM_APP + 38,
    query_graphics_fallback = WM_APP + 39,
    query_microsteps = WM_APP + 40,
    query_presentation_rate = WM_APP + 41,
    query_remaining_microsteps = WM_APP + 42,
    query_completed_ticks = WM_APP + 43,
    query_raw_keyboard = WM_APP + 44,
    query_sprite_antialiasing = WM_APP + 45,
};
template <class Host> std::optional<LRESULT> inspect(const Host& host, UINT message, WPARAM w) {
    switch (message) {
        case query_sprite_antialiasing:
            return host.renderer && host.renderer->antialias_sprites();
        case query_raw_keyboard:
            return unsigned(host.raw_keyboard_registered) | (unsigned(host.raw_keyboard.active()) << 1);
        case query_microsteps:
            return LRESULT(host.incremental.microsteps());
        case query_presentation_rate:
            return host.edition.incremental_presentation ? host.presentation_clock.rate() : 60;
        case query_remaining_microsteps:
            return host.game ? host.game->remaining_microsteps() : 0;
        case query_completed_ticks:
            return LRESULT(host.incremental.ticks());
        case query_graphics_fallback:
            return host.table_presentation.using_fallback() ? 1 : 0;
        case query_gpu_presentation:
            return host.gpu_presented ? 1 : 0;
        case query_timed_input_frames:
            return LRESULT(host.timed_input_frames);
        case query_input_active_microsteps:
            return w < host.input_active_microsteps.size() ? host.input_active_microsteps[w] : -1;
        case query_depth_view:
            return host.show_depth ? 1 : 0;
        case query_game_frames:
            return host.game ? LRESULT(host.game->frames()) : -1;
        case query_game_phase:
            return host.game ? LRESULT(unsigned(host.game->phase()) | (host.game->ball_number() << 8)) : -1;
        case query_presented_frames:
            return LRESULT(host.timing.frames());
        case query_overlay_visible:
            return host.diagnostics.overlay ? 1 : 0;
        case query_high_resolution_timer:
            return host.high_resolution_timer ? 1 : 0;
        case query_pixel_scale:
            return host.pixel_scale;
        case query_audio_buffers:
            return host.audio ? LRESULT(host.audio->submitted_buffers()) : -1;
        case query_configuration_word:
            return host.game && w < host.options.configuration().size() ? LRESULT(host.options.configuration()[w]) : -1;
        case query_actions:
            return host.game ? (host.actions | host.pad_actions) & ~web::host_action::commands : -1;
        case query_players:
            return host.game ? LRESULT(host.game->player_count() | (host.game->current_player() << 8)) : -1;
        case query_variant:
            return host.variant;
        case query_raster_counter:
            return host.renderer ? LRESULT(host.renderer->raster_counter()) : -1;
        case query_dmd_pixels:
            if (host.game && w < web::DotMatrixFrame::pixel_count / 4) {
                const auto& pixels = host.game->dot_matrix_frame().pixels;
                return pixels[w * 4] | pixels[w * 4 + 1] << 8 | pixels[w * 4 + 2] << 16 | pixels[w * 4 + 3] << 24;
            }
            return -1;
        case query_music_track:
            return host.music ? LRESULT(host.music->cd_track()) : -1;
        case query_music_frames:
            return host.music ? LRESULT(host.music->submitted_frames()) : -1;
        case query_console_attached:
            return GetConsoleWindow() ? 1 : 0;
        case query_startup_ticks:
            return host.game ? LRESULT(host.game->startup_ticks()) : -1;
        case query_audio_rate:
            return host.audio ? LRESULT(host.audio->rate()) : 0;
        case query_audio_channels:
            return host.audio ? LRESULT(host.audio->channels()) : 0;
        case query_audio_format:
            return host.audio ? LRESULT(host.audio->format().channels() | host.audio->format().bits() << 8) : 0;
        case query_nudge_presentations:
            return LRESULT(host.nudge_presentations);
        case query_exit_display:
            return host.exit_display_prepared;
        case query_fullscreen:
            return host.display.fullscreen;
        case query_integer_scaling:
            return host.display.scaling == web::PresentationScaling::integer;
        case query_nudge_x:
            return host.options.visual_nudge() ? host.nudge->offset(host.variant).x : 0;
        case query_nudge_y:
            return host.options.visual_nudge() ? host.nudge->offset(host.variant).y : 0;
        case query_rejected_starts:
            return host.game ? LRESULT(host.game->rejected_starts()) : 0;
        case query_restart_pending:
            return host.game && host.game->restart_pending();
        case query_active_balls:
            return host.game ? std::count_if(host.game->simulation().balls.begin(), host.game->simulation().balls.end(), [](const auto& ball) { return ball.active() != 0; }) : 0;
        case query_ball_position:
            if (host.game && w < host.game->simulation().balls.size()) {
                const auto position = host.game->simulation().balls[w].position();
                return LRESULT(std::uint32_t(std::uint16_t(position.x >> 16)) | std::uint32_t(std::uint16_t(position.y >> 16)) << 16);
            }
            return -1;
        case query_ball_z:
            return host.game && w < host.game->simulation().balls.size() ? host.game->simulation().balls[w].z() : 0;
        case query_waiting_launch:
            return host.game && host.game->rule_state().launch_enabled && std::any_of(host.game->simulation().balls.begin(), host.game->simulation().balls.end(), [](const auto& ball) {
                       return ball.active() && ball.motion_type() == web::physics::ball_type::launcher && ball.transition_kind() == 16;
                   });
        case query_launches:
            return host.game ? LRESULT(host.game->launches()) : 0;
        case query_music_starts:
            return host.music ? LRESULT(host.music->playback_starts()) : 0;
    }
    return {};
}
} // namespace web::development
