#pragma once
#include "player_record.hpp"
#include <algorithm>
#include <utility>
namespace web::compatibility {
inline std::array<std::uint32_t, 11> saved_initials_tail(const rules::SharedGameState& state) {
    std::array<std::uint32_t, 11> result{};
    for (unsigned i = 0; i < 11; ++i)
        result[i] = std::uint32_t(state.players().at((i + 1) / 3).records.saved_initials[(i + 1) % 3]);
    return result;
}
using rules::SharedGameState;
using rules::TurnState;
inline constexpr std::size_t turn_snapshot_bytes = 1072, session_snapshot_bytes = 4068;
enum class TurnField : std::uint32_t {
    // Identifiers are preferred-image virtual addresses in original WGAME,
    // not native pointers. Keep address arithmetic inside the state store.
    serve_spacing_timer = 0x431864,
    switch_previous_first = 0x4318a4,
    switch_edge_first = 0x431958,
    switch_contact_first = 0x431a08,
    station_centre_stage = 0x4317fc,
    left_loop_completed = 0x431800,
    station_super_stage = 0x431808,
    magnet_voice_timer = 0x43180c,
    station_ramp_ready = 0x431810,
    left_loop_completion_timer = 0x431814,
    right_spinner_timer = 0x431818,
    magnet_combo_timer = 0x43181c,
    inlane_a_timer = 0x431820,
    magnet_activation_window = 0x431824,
    left_spinner_lane_timer = 0x431828,
    inlane_b_timer = 0x43182c,
    station_triple_ready = 0x431830,
    right_spinner_lane_timer = 0x431838,
    loop_completed = 0x43184c,
    loop_first_gate_timer = 0x43186c,
    super_rescue = 0x431870,
    diverter_warning = 0x431880,
    loop_last_gate_timer = 0x431888,
    station_super_lit = 0x431898,
    loop_remaining = 0x4317f8,
    record_scan_index = 0x431b48,
    record_best_high = 0x431b50,
    record_rank = 0x431b54,
    record_best_low = 0x431b58,
    showdown_sound_timer = 0x431b5c,
    record_best_player = 0x431b60,
    showdown_finish_timer = 0x431b64,
    initials_confirmation_timer = 0x431b68,
    match_timer = 0x431b6c,
    record_maximum = 0x431b70,
    showdown_acceleration = 0x431b74,
    kickback_announcement_played = 0x431b78,
    showdown_sound_phase = 0x431b84,
    music_fade_timer = 0x431b8c,
    record_flow_stage = 0x431b94,
    bonus_display_gate = record_flow_stage,
    music_requested_track = 0x431b98,
    match_awarded = 0x431ba0,
    lamp_previous_frame_phase = 0x431ba8,
    super_jet_total = 0x431bcc,
    match_digit = 0x431bec,
    match_stage = 0x431bf4,
    loop_awarded = 0x431bf8,
    super_skillshot = loop_awarded, // Original shared award guard.
    initials_cursor = 0x431bfc,
    record_display_rank = 0x431c04,
    record_entry_player = 0x431c08,
    drain_gate = 0x4317f0,
    mission_time = drain_gate, // The original uses the same word for its mission countdown.
    mission_total = 0x431bc8,
    mission_flash = 0x431abc,
    mission_flash_time = 0x431ad8,
    mission_intro = 0x431adc,
    mission_started_this_frame = 0x431840,
    centre_award_consumed = 0x431954,
    centre_award_wait = 0x431afc,
    skyscraper_floor = 0x431be4,
    save_timer = 0x431848,
    auto_launch_block = 0x431850,
    auto_kick_enabled = 0x431854,
    return_toggle = 0x431868,
    serve_requests = 0x431878,
    diverter_enabled = 0x43187c,
    positive_sling_inhibit = 0x4318cc,
    negative_sling_inhibit = 0x4318d0,
    tilt_lock = 0x431b0c,
    drain_extension = 0x431ae8,
    phase = 0x431af0,
    status_right_hold = 0x431af8,
    status_left_hold = 0x431b3c,
    status_selection = 0x431b08,
    status_timer = 0x431b10,
    launch_timer = 0x431b14,
    elapsed = 0x431b20,
    bonus_stage = 0x431b30,
    bonus_timer = 0x431c00,
    continue_timer = 0x431b1c,
    extra_ball_turn = 0x431884,
    extra_ball_diverter = 0x43188c,
    outlane_extra_ball = 0x431ab8,
    kickback_rearm_timer = 0x4317f4,
    web_ramp_entry_timer = 0x43183c,
    ramp_a_entry_timer = 0x43189c,
    drain_policy = 0x431b28,
    single_ball_award_mode = drain_policy, // 0 none, 1 ready, 2 active; also gates drain handling.
    current_player = 0x431ba4,
    launch_enabled = 0x431bc0,
    video_active = 0x431bb4,
    video_lock_pending = 0x431bd8,
    lock_choice_timer = 0x431b00,
    lock_choice = 0x431bdc,
    explosion_wait = 0x431ae0,
    flipper_timing_window = 0x431b88,
    super_jackpot = 0x431890,
    combined_lock = 0x431aec,
    award_retrigger_guard = 0x431b90,
    super_launcher_timer = 0x431ac0,
    super_launcher_hits = 0x431ac8,
    super_launcher_countdown = 0x431acc,
    quick_shot_value = 0x431858,
    quick_shot_timer = 0x43185c,
    quick_shot_warning = 0x431874,
    quick_shot_side = 0x431894,
    tilt_gate = 0x431ad4,
    showdown_stage = tilt_gate, // Shared by original tilt guard and Showdown progression.
    showdown_phase = 0x431ad0,
    showdown_hits = 0x4318a0,
    showdown_artwork = 0x431b4c,
    showdown_intro = 0x431be8,
    showdown_intro_timer = 0x431b9c,
    showdown_reserves = 0x431be0,
    showdown_total = 0x431bd0,
    showdown_rotating_lamp = 0x431ae4,
    warnings_left = 0x431af4,
    tilted = 0x431b24,
    lamp_animation = 0x431b7c,
    lamp_mode = 0x431b44,
    lamp_frame = 0x431b38,
    player_count = 0x431bb8,
    start_latch = 0x431bac,
    startup_display_ticks = 0x431bd4,
    player_turn = 0x431bb0,
    player_cycle = 0x431860,
    announce_pending = 0x431844,
    announcement_voice = 0x431a04,
    lamp_queue = 0x431950,
    target_bank_a_timer = 0x431ac4,
    target_bank_b_timer = 0x431ab4,
    returns_paused = 0x431bc4,
    drain_override = 0x431bf0,
    live_balls = 0x431c0c
};
enum class SessionField : std::uint32_t {
    first_drop_target_down = 0x431208,
    lamp_queue_timer_first = 0x4311e8,
    lamp_queue_output_first = 0x431220,
    first_saved_initial = 0x431240,
    web_ramp_combo_timer = 0x431778,
    jet_gate_timer = 0x43177c,
    right_combo_entry_timer = 0x431784,
    right_loop_lamp_timer = 0x431790,
    jet_completion_window = 0x431798,
    left_loop_lamp_timer = 0x43179c,
    combo_mania_timer = 0x4317b8,
    combo_count = 0x4317bc,
    combo_remaining = 0x4317c8,
    left_combo_entry_timer = 0x4317cc,
    left_spinner_timer = 0x4317dc,
    station_super_holdoff = 0x4317e0,
    station_super_window = 0x4317e4,
    right_loop_completed = 0x4317e8,
    right_loop_completion_timer = 0x4317ec,
    initials_initialized = 0x431c10,
    loop_count = 0x431c14,
    initials_letters = 0x431c18,
    lamp_animation_enabled = 0x42c4e8,
    warning_display = 0x447dbc,
    music_state = 0x42c964,
    dmd_queue_count = 0x444d44,
    dmd_queue_state = 0x444d34,
    dmd_queue_aux = 0x42f4dc,
    dmd_animation = 0x447dc4,
    dmd_frame = 0x447dac,
    dmd_status = 0x447de0,
    dmd_aux = 0x447dcc,
    mission_kind = 0x4317c4,
    mission_centre_lit = 0x4317a0,
    reactor_remaining = 0x4317a4,
    reactor_regeneration = 0x4317ac,
    hunt_remaining = 0x431794,
    hunt_target = 0x4317c0,
    ammo_remaining = 0x4317b0,
    ammo_stage = 0x4317a8,
    ammo_strength = 0x431788,
    shuttle_hits = 0x4317b4,
    shuttle_charge = 0x43178c,
    skyscraper_cooldown = 0x4317d0,
    skyscraper_extended = 0x431780
};
inline void put(Bytes& data, std::size_t offset, std::int32_t value) {
    for (unsigned byte = 0; byte < 4; ++byte)
        data.at(offset + byte) = std::uint8_t(std::uint32_t(value) >> (byte * 8));
}
template <class State> inline auto& turn_word(State& state, std::uint32_t address) {
    switch (address) {
        case 4397040:
            return state.mission_time;
        case 4397044:
            return state.kickback_rearm_timer;
        case 4397048:
            return state.loop_remaining;
        case 4397052:
            return state.station_centre_stage;
        case 4397056:
            return state.left_loop_completed;
        case 4397060:
            return state.reserved_0;
        case 4397064:
            return state.station_stage_code;
        case 4397068:
            return state.magnet_voice_timer;
        case 4397072:
            return state.station_ramp_ready;
        case 4397076:
            return state.left_loop_completion_timer;
        case 4397080:
            return state.right_spinner_timer;
        case 4397084:
            return state.magnet_combo_timer;
        case 4397088:
            return state.inlane_a_timer;
        case 4397092:
            return state.magnet_activation_window;
        case 4397096:
            return state.left_spinner_lane_timer;
        case 4397100:
            return state.inlane_b_timer;
        case 4397104:
            return state.station_triple_ready;
        case 4397108:
            return state.reserved_1;
        case 4397112:
            return state.right_spinner_lane_timer;
        case 4397116:
            return state.web_ramp_entry_timer;
        case 4397120:
            return state.mission_started_this_frame;
        case 4397124:
            return state.announce_pending;
        case 4397128:
            return state.save_timer;
        case 4397132:
            return state.loop_completed;
        case 4397136:
            return state.auto_launch_block;
        case 4397140:
            return state.auto_kick_enabled;
        case 4397144:
            return state.quick_shot_value;
        case 4397148:
            return state.quick_shot_timer;
        case 4397152:
            return state.player_cycle;
        case 4397156:
            return state.serve_spacing_timer;
        case 4397160:
            return state.return_toggle;
        case 4397164:
            return state.loop_first_gate_timer;
        case 4397168:
            return state.super_rescue;
        case 4397172:
            return state.quick_shot_warning;
        case 4397176:
            return state.serve_requests;
        case 4397180:
            return state.diverter_enabled;
        case 4397184:
            return state.diverter_warning;
        case 4397188:
            return state.extra_ball_turn;
        case 4397192:
            return state.loop_last_gate_timer;
        case 4397196:
            return state.extra_ball_diverter;
        case 4397200:
            return state.super_jackpot;
        case 4397204:
            return state.quick_shot_side;
        case 4397208:
            return state.station_super_lit;
        case 4397212:
            return state.ramp_a_entry_timer;
        case 4397216:
            return state.showdown_hits;
        case 4397260:
            return state.positive_sling_inhibit;
        case 4397264:
            return state.negative_sling_inhibit;
        case 4397392:
            return state.lamp_queue;
        case 4397396:
            return state.centre_award_consumed;
        case 4397572:
            return state.announcement_voice;
        case 4397748:
            return state.target_bank_b_timer;
        case 4397752:
            return state.outlane_extra_ball;
        case 4397756:
            return state.mission_flash;
        case 4397760:
            return state.super_launcher_timer;
        case 4397764:
            return state.target_bank_a_timer;
        case 4397768:
            return state.super_launcher_hits;
        case 4397772:
            return state.super_launcher_countdown;
        case 4397776:
            return state.showdown_phase;
        case 4397780:
            return state.showdown_stage;
        case 4397784:
            return state.mission_flash_time;
        case 4397788:
            return state.mission_intro;
        case 4397792:
            return state.explosion_wait;
        case 4397796:
            return state.showdown_rotating_lamp;
        case 4397800:
            return state.drain_extension;
        case 4397804:
            return state.combined_lock;
        case 4397808:
            return state.phase_code;
        case 4397812:
            return state.warnings_left;
        case 4397816:
            return state.status_right_hold;
        case 4397820:
            return state.centre_award_wait;
        case 4397824:
            return state.lock_choice_timer;
        case 4397832:
            return state.status_selection;
        case 4397836:
            return state.tilt_lock;
        case 4397840:
            return state.status_timer;
        case 4397844:
            return state.launch_timer;
        case 4397848:
            return state.reserved_2;
        case 4397852:
            return state.continue_timer;
        case 4397856:
            return state.elapsed;
        case 4397860:
            return state.tilted;
        case 4397864:
            return state.single_ball_award_mode;
        case 4397868:
            return state.reserved_3;
        case 4397872:
            return state.bonus_stage;
        case 4397876:
            return state.reserved_4;
        case 4397880:
            return state.lamp_frame;
        case 4397884:
            return state.status_left_hold;
        case 4397888:
            return state.reserved_5;
        case 4397892:
            return state.lamp_mode;
        case 4397896:
            return state.record_scan_index;
        case 4397900:
            return state.showdown_artwork;
        case 4397904:
            return state.record_best_high;
        case 4397908:
            return state.record_rank;
        case 4397912:
            return state.record_best_low;
        case 4397916:
            return state.showdown_sound_timer;
        case 4397920:
            return state.record_best_player;
        case 4397924:
            return state.showdown_finish_timer;
        case 4397928:
            return state.initials_confirmation_timer;
        case 4397932:
            return state.match_timer;
        case 4397936:
            return state.record_maximum;
        case 4397940:
            return state.showdown_acceleration;
        case 4397944:
            return state.kickback_announcement_played;
        case 4397948:
            return state.lamp_animation;
        case 4397952:
            return state.reserved_6;
        case 4397956:
            return state.showdown_sound_phase;
        case 4397960:
            return state.flipper_timing_window;
        case 4397964:
            return state.music_fade_timer;
        case 4397968:
            return state.award_retrigger_guard;
        case 4397972:
            return state.bonus_display_gate;
        case 4397976:
            return state.music_requested_track;
        case 4397980:
            return state.showdown_intro_timer;
        case 4397984:
            return state.match_awarded;
        case 4397988:
            return state.current_player;
        case 4397992:
            return state.lamp_previous_frame_phase;
        case 4397996:
            return state.start_latch;
        case 4398000:
            return state.player_turn;
        case 4398004:
            return state.video_active;
        case 4398008:
            return state.player_count;
        case 4398016:
            return state.launch_enabled;
        case 4398020:
            return state.returns_paused;
        case 4398024:
            return state.mission_total;
        case 4398028:
            return state.super_jet_total;
        case 4398032:
            return state.showdown_total;
        case 4398036:
            return state.startup_display_ticks;
        case 4398040:
            return state.video_lock_pending;
        case 4398044:
            return state.lock_choice;
        case 4398048:
            return state.showdown_reserves;
        case 4398052:
            return state.skyscraper_floor;
        case 4398056:
            return state.showdown_intro;
        case 4398060:
            return state.match_digit;
        case 4398064:
            return state.drain_override;
        case 4398068:
            return state.match_stage;
        case 4398072:
            return state.super_skillshot;
        case 4398076:
            return state.initials_cursor;
        case 4398080:
            return state.bonus_timer;
        case 4398084:
            return state.record_display_rank;
        case 4398088:
            return state.record_entry_player;
        case 4398092:
            return state.live_balls;
        default:
            break;
    }
    if (address % 4)
        throw std::out_of_range("Unaligned legacy turn field");
    if (address >= 0x4318a4 && address < 0x4318a4 + 43 * 4)
        return state.previous_switch((address - 0x4318a4) / 4);
    if (address >= 0x431958 && address < 0x431958 + 43 * 4)
        return state.edge_switch((address - 0x431958) / 4);
    if (address >= 0x431a08 && address < 0x431a08 + 64 * 4)
        return state.contact_switch((address - 0x431a08) / 4);
    throw std::out_of_range("Unknown legacy turn field");
}
template <class State> inline auto& session_word(State& state, std::uint32_t address) {
    switch (address) {
        case 4375784:
            return state.lamp_animation_enabled;
        case 4376932:
            return state.music_track;
        case 4388060:
            return state.dmd_queue_aux;
        case 4395584:
            return state.players().at(0).records.saved_initials[0];
        case 4396920:
            return state.web_ramp_combo_timer;
        case 4396924:
            return state.jet_gate_timer;
        case 4396928:
            return state.skyscraper_extended;
        case 4396932:
            return state.right_combo_entry_timer;
        case 4396936:
            return state.ammo_strength;
        case 4396940:
            return state.shuttle_charge;
        case 4396944:
            return state.right_loop_lamp_timer;
        case 4396948:
            return state.hunt_remaining;
        case 4396952:
            return state.jet_completion_window;
        case 4396956:
            return state.left_loop_lamp_timer;
        case 4396960:
            return state.mission_centre_lit;
        case 4396964:
            return state.reactor_remaining;
        case 4396968:
            return state.ammo_stage;
        case 4396972:
            return state.reactor_regeneration;
        case 4396976:
            return state.ammo_remaining;
        case 4396980:
            return state.shuttle_hits;
        case 4396984:
            return state.combo_mania_timer;
        case 4396988:
            return state.combo_count;
        case 4396992:
            return state.hunt_target;
        case 4396996:
            return state.mission_kind;
        case 4397000:
            return state.combo_remaining;
        case 4397004:
            return state.left_combo_entry_timer;
        case 4397008:
            return state.skyscraper_cooldown;
        case 4397012:
            return state.skyscraper_reset;
        case 4397016:
            return state.mode_reset;
        case 4397020:
            return state.left_spinner_timer;
        case 4397024:
            return state.station_super_holdoff;
        case 4397028:
            return state.station_super_window;
        case 4397032:
            return state.right_loop_completed;
        case 4397036:
            return state.right_loop_completion_timer;
        case 4398096:
            return state.initials_initialized;
        case 4398100:
            return state.loop_count;
        case 4398104:
            return state.initials_letters;
        case 4476212:
            return state.dmd_queue_state;
        case 4476228:
            return state.dmd_queue_count;
        case 4488620:
            return state.dmd_frame;
        case 4488636:
            return state.warning_display;
        case 4488644:
            return state.dmd_animation;
        case 4488652:
            return state.dmd_aux;
        case 4488672:
            return state.dmd_status;
        case 0x431208:
            return state.lamps.drop_targets[0];
        default:
            break;
    }
    if (address % 4)
        throw std::out_of_range("Unaligned legacy session field");
    if (address >= 0x4311e8 && address < 0x431208)
        return state.lamp_queue.at((address - 0x4311e8) / 4).remaining;
    if (address >= 0x431220 && address < 0x431240)
        return state.lamp_queue.at((address - 0x431220) / 4).output;
    throw std::out_of_range("Unknown legacy session field");
}
inline std::int32_t get(const TurnState& state, TurnField field) {
    return turn_word(state, std::uint32_t(field));
}
inline void set(TurnState& state, TurnField field, std::int32_t value) {
    turn_word(state, std::uint32_t(field)) = value;
}
inline std::int32_t get(const SharedGameState& state, SessionField field) {
    return session_word(state, std::uint32_t(field));
}
inline void set(SharedGameState& state, SessionField field, std::int32_t value) {
    if (field == SessionField::music_state)
        state.request_music_track(value);
    else
        session_word(state, std::uint32_t(field)) = value;
}
template <class State, class Field> inline auto get(const State& state, Field field) {
    return state.get(field);
}
template <class State, class Field, class Value>
    requires requires(State&& state, Field field, Value value) { state.set(field, value); }
inline void set(State&& state, Field field, Value value) {
    state.set(field, value);
}
inline SessionField lamp_queue_timer_field(unsigned index) {
    return SessionField(0x4311e8 + index * 4);
}
inline SessionField lamp_queue_output_field(unsigned index) {
    return SessionField(0x431220 + index * 4);
}
inline Bytes encode_turn(const TurnState& value) {
    const auto& state = value;
    Bytes result(turn_snapshot_bytes);
    std::size_t offset = 0;
    for (std::uint32_t address = 0x4317f0; address < 0x431c10; address += 4) {
        if (address == 0x431bbc)
            continue;
        put(result, offset, turn_word(state, address));
        offset += 4;
    }
    for (unsigned player = 0; player < 4; ++player)
        put(result, 1052 + player * 4, state.players().at(player).ball.relaunch);
    put(result, 1068, physics::integer::bits(state.frame_phase()));
    return result;
}
inline void decode_turn(TurnState& state, std::span<const std::uint8_t> data) {
    if (data.size() != turn_snapshot_bytes)
        throw std::invalid_argument("Invalid turn snapshot length");
    std::size_t offset = 0;
    for (std::uint32_t address = 0x4317f0; address < 0x431c10; address += 4) {
        if (address == 0x431bbc)
            continue;
        turn_word(state, address) = physics::integer::bits(u32(data, offset));
        offset += 4;
    }
    for (unsigned player = 0; player < 4; ++player)
        state.players().at(player).ball.relaunch = physics::integer::bits(u32(data, 1052 + player * 4));
    state.frame_phase(u32(data, 1068));
}
inline constexpr std::array<std::pair<std::uint32_t, std::size_t>, 4> session_ranges = {
    {{0x4311e8, 36}, {0x431220, 36}, {0x431778, 120}, {0x431c10, 12}}
};
inline constexpr std::array<std::uint32_t, 10> session_extras = {0x42c4e8, 0x447dbc, 0x42c964, 0x444d44, 0x444d34, 0x42f4dc, 0x447dc4, 0x447dac, 0x447de0, 0x447dcc};
inline Bytes encode_session(const SharedGameState& value) {
    const auto& state = value;
    Bytes result(session_snapshot_bytes);
    for (unsigned slot = 0; slot < 4; ++slot)
        for (std::size_t field = 0; field < 308; field += 4)
            if (field != 168)
                put(result, slot * 304 + field - (field > 168 ? 4 : 0), legacy_player_word(state.players().at(slot), field));
    std::copy(state.lamps.animation_cells.begin(), state.lamps.animation_cells.end(), result.begin() + 1216);
    for (unsigned i = 0; i < 70; ++i) {
        put(result, 3264 + i * 4, physics::integer::bits(state.lamps.sample_indices[i]));
        put(result, 3544 + i * 4, state.lamps.outputs[i]);
    }
    std::size_t offset = 3824;
    for (auto [first, count] : session_ranges)
        for (unsigned i = 0; i < count; i += 4) {
            put(result, offset, session_word(state, first + i));
            offset += 4;
        }
    for (auto address : session_extras) {
        put(result, offset, session_word(state, address));
        offset += 4;
    }
    return result;
}
inline void decode_session(SharedGameState& state, std::span<const std::uint8_t> data) {
    if (data.size() != session_snapshot_bytes)
        throw std::invalid_argument("Invalid session snapshot length");
    for (unsigned slot = 0; slot < 4; ++slot)
        for (std::size_t field = 0; field < 308; field += 4)
            if (field != 168)
                legacy_player_word(state.players().at(slot), field) = physics::integer::bits(u32(data, slot * 304 + field - (field > 168 ? 4 : 0)));
    std::copy_n(data.begin() + 1216, 2048, state.lamps.animation_cells.begin());
    for (unsigned i = 0; i < 70; ++i) {
        state.lamps.sample_indices[i] = u32(data, 3264 + i * 4);
        state.lamps.outputs[i] = physics::integer::bits(u32(data, 3544 + i * 4));
    }
    std::size_t offset = 3824;
    for (auto [first, count] : session_ranges)
        for (unsigned i = 0; i < count; i += 4) {
            session_word(state, first + i) = physics::integer::bits(u32(data, offset));
            offset += 4;
        }
    for (auto address : session_extras) {
        session_word(state, address) = physics::integer::bits(u32(data, offset));
        offset += 4;
    }
}
inline Bytes encode(const TurnState& state) {
    return encode_turn(state);
}
inline Bytes encode(const SharedGameState& state) {
    return encode_session(state);
}
inline TurnState read_turn(rules::PlayerRoster& players, std::span<const std::uint8_t> bytes) {
    TurnState state(players);
    decode_turn(state, bytes);
    return state;
}
inline SharedGameState read_session(rules::PlayerRoster& players, std::span<const std::uint8_t> bytes) {
    SharedGameState state(players);
    decode_session(state, bytes);
    return state;
}
} // namespace web::compatibility
