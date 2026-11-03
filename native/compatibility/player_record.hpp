#pragma once
#include "../rules/session_rules.hpp"
namespace web::compatibility {
using rules::Player;
template <class PlayerState> inline auto& legacy_player_word(PlayerState& player, std::size_t offset) {
    switch (offset) {
        case 0:
            return player.ball.number;
        case 4:
            return player.score.units;
        case 8:
            return player.score.rollovers;
        case 12:
            return player.awards.skillshot_value;
        case 16:
            return player.records.replay_awarded;
        case 20:
            return player.records.buyins;
        case 24:
            return player.bonus.maxed;
        case 28:
            return player.awards.extra_ball_awards;
        case 32:
            return player.missions.played[0];
        case 36:
            return player.missions.played[1];
        case 40:
            return player.missions.played[2];
        case 44:
            return player.missions.played[3];
        case 48:
            return player.missions.played[4];
        case 52:
            return player.missions.played[5];
        case 56:
            return player.missions.sphere;
        case 60:
            return player.combos.sphere;
        case 64:
            return player.bonus.sphere;
        case 68:
            return player.station.sphere;
        case 72:
            return player.power.sphere;
        case 76:
            return player.kickback.first_use_voice;
        case 80:
            return player.missions.selected;
        case 84:
            return player.compatibility.reset_word_a;
        case 88:
            return player.compatibility.reset_word_b;
        case 92:
            return player.missions.all_played;
        case 96:
            return player.missions.rounds;
        case 100:
            return player.ball.extra_balls;
        case 104:
            return player.awards.extra_balls_lit;
        case 108:
            return player.loops.extra_ball_threshold;
        case 112:
            return player.showdown.spheres;
        case 116:
            return player.showdown.rounds;
        case 120:
            return player.power.level;
        case 124:
            return player.bonus.power_levels;
        case 128:
            return player.compatibility.initialization_word;
        case 132:
            return player.launcher.state;
        case 136:
            return player.launcher.rounds;
        case 140:
            return player.magnet.ready;
        case 144:
            return player.magnet.hold;
        case 148:
            return player.awards.explosion_ready;
        case 152:
            return player.lock.state;
        case 156:
            return player.lock.balls;
        case 160:
            return player.lock.progress;
        case 164:
            return player.lock.rounds;
        case 168:
            return player.ball.relaunch;
        case 172:
            return player.awards.ramp_value;
        case 176:
            return player.station.state;
        case 180:
            return player.station.captures;
        case 184:
            return player.arena.ramps;
        case 188:
            return player.arena.extra_ball_at;
        case 192:
            return player.arena.lock_at;
        case 196:
            return player.bonus.loops;
        case 200:
            return player.loops.best;
        case 204:
            return player.combos.best;
        case 208:
            return player.combos.remaining;
        case 212:
            return player.bonus.combos;
        case 216:
            return player.combos.rounds;
        case 220:
            return player.combos.state;
        case 224:
            return player.compatibility.retained_word;
        case 228:
            return player.quick_shot.state;
        case 232:
            return player.quick_shot.count;
        case 236:
            return player.video.perfect_rounds;
        case 240:
            return player.video.ready;
        case 244:
            return player.video.rounds;
        case 248:
            return player.video.extra_ball_cycles;
        case 252:
            return player.kickback.available;
        case 256:
            return player.targets[0];
        case 260:
            return player.targets[1];
        case 264:
            return player.targets[2];
        case 268:
            return player.targets[3];
        case 272:
            return player.targets[4];
        case 276:
            return player.targets[5];
        case 280:
            return player.bumpers.super_jets;
        case 284:
            return player.bumpers.hold;
        case 288:
            return player.bonus.hold;
        case 292:
            return player.rollovers[0];
        case 296:
            return player.rollovers[1];
        case 300:
            return player.rollovers[2];
        case 304:
            return player.bonus.multiplier;
        default:
            throw std::out_of_range("Invalid player field");
    }
}
// Transitional compatibility view; the Player owns every semantic field.
class PlayerRecord {
public:
    static constexpr std::size_t byte_count = 308, owned_bytes = 304;
    enum class Field : std::size_t {
        combo_sphere = 0x3c,
        station_sphere = 0x44,
        kickback_first_use_voice = 0x4c,
        super_launcher_rounds = 0x88,
        magnetic_ready = 0x8c,
        station_captures = 0xb4,
        drop_target_first = 0x100,
        mission_played_first = 0x20,
        ball_number = 0,
        score = 4,
        score_rollovers = 8,
        skill_shot_value = 0x0c,
        replay_awarded = 0x10,
        buyins = 0x14,
        bonus_maxed = 0x18,
        extra_ball_awards = 0x1c,
        bonus_sphere = 0x40,
        power_sphere = 0x48,
        mission_sphere = 0x38,
        mission_selection = 0x50,
        all_missions_played = 0x5c,
        mission_rounds = 0x60,
        extra_balls = 0x64,
        extra_balls_lit = 0x68,
        extra_ball_loop_threshold = 0x6c,
        showdown_spheres = 0x70,
        showdown_rounds = 0x74,
        spinner_value = 0x78,
        power_level = spinner_value,
        bonus_power_levels = 0x7c,
        super_launcher = 0x84,
        magnetic_hold = 0x90,
        explosion_ready = 0x94,
        lock_state = 0x98,
        lock_balls = 0x9c,
        lock_progress = 0xa0,
        lock_rounds = 0xa4,
        relaunch = 0xa8,
        ramp_value = 0xac,
        ramp_award_state = 0xb0,
        station_state = ramp_award_state,
        arena_ramps = 0xb8,
        arena_extra_ball_at = 0xbc,
        arena_lock_at = 0xc0,
        bonus_loops = 0xc4,
        best_loop = 0xc8,
        best_combo = 0xcc,
        combo_mania_remaining = 0xd0,
        bonus_combos = 0xd4,
        combo_mania_rounds = 0xd8,
        combo_mania = 0xdc,
        quick_shot = 0xe4,
        quick_shot_count = 0xe8,
        video_mode_ready = 0xf0,
        video_perfect_rounds = 0xec,
        video_rounds = 0xf4,
        video_extra_ball_cycles = 0xf8,
        kickback_available = 0xfc,
        super_jets = 0x118,
        hold_superjets = 0x11c,
        hold_bonus = 0x120,
        rollover_left = 0x124,
        rollover_middle = 0x128,
        rollover_right = 0x12c,
        bonus_multiplier = 0x130
    };
    // Indexed fields retain the original four-byte stride at this boundary.
    static constexpr Field drop_target_field(unsigned target) {
        return Field(std::size_t(Field::drop_target_first) + target * sizeof(std::int32_t));
    }
    static constexpr Field mission_played_field(unsigned mission) {
        return Field(std::size_t(Field::mission_played_first) + mission * sizeof(std::int32_t));
    }
    static constexpr Field rollover_field(unsigned rollover) {
        return Field(std::size_t(Field::rollover_left) + rollover * sizeof(std::int32_t));
    }
    std::int32_t get(std::size_t offset) const {
        return legacy_player_word(player_, offset);
    }
    void set(std::size_t offset, std::int32_t value) {
        legacy_player_word(player_, offset) = value;
    }
    std::int32_t get(Field field) const {
        return get(std::size_t(field));
    }
    void set(Field field, std::int32_t value) {
        set(std::size_t(field), value);
    }

public:
    explicit PlayerRecord(Player& player) :
        player_(player) {}
    operator Player&() const {
        return player_;
    }
    Player& player_;
};

inline PlayerRecord player(rules::SharedGameState& state, std::size_t slot) {
    return PlayerRecord(state.players().at(unsigned(slot)));
}
inline std::int32_t player_word(const rules::SharedGameState& state, std::size_t slot, PlayerRecord::Field field) {
    if (field == PlayerRecord::Field::relaunch)
        throw std::out_of_range("Relaunch is excluded from legacy session bytes");
    return legacy_player_word(state.players().at(unsigned(slot)), std::size_t(field));
}
} // namespace web::compatibility
