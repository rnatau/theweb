#pragma once
#include "score.hpp"
#include <array>
#include <optional>
#include <stdexcept>
namespace web::rules {
struct PlayerId {
    unsigned slot = 0;
    bool operator==(const PlayerId&) const = default;
};
// One owning player. No field depends on another object's byte layout.
class Player {
public:
    explicit Player(PlayerId id = {}) :
        id_(id) {
        if (id.slot == 0)
            records.saved_initials[0] = 0;
    }
    PlayerId id() const {
        return id_;
    }
    void initialize(std::int32_t mission_selection) {
        const auto retained_power_level = power.level;
        const auto retained_mode_word = compatibility.retained_word;
        const auto eligibility = records;
        *this = Player(id_);
        power.level = retained_power_level;
        compatibility.retained_word = retained_mode_word;
        records.processed = eligibility.processed;
        records.pre_buyin = eligibility.pre_buyin;
        records.pre_buyin_processed = eligibility.pre_buyin_processed;
        records.confirmed_initials = eligibility.confirmed_initials;
        records.saved_initials = eligibility.saved_initials;
        ball.number = loops.best = combos.best = magnet.hold = arena.lock_at = kickback.available = bonus.multiplier = kickback.first_use_voice = 1;
        awards.ramp_value = 10000000;
        loops.extra_ball_threshold = 2;
        awards.skillshot_value = 25000000;
        arena.extra_ball_at = 20;
        combos.remaining = 15;
        missions.selected = mission_selection;
    }
    std::array<std::int32_t*, 5> qualification_spheres() {
        return {&missions.sphere, &combos.sphere, &bonus.sphere, &station.sphere, &power.sphere};
    }
    std::array<const std::int32_t*, 5> qualification_spheres() const {
        return {&missions.sphere, &combos.sphere, &bonus.sphere, &station.sphere, &power.sphere};
    }
    PlayerScore score;
    struct BallProgress {
        std::int32_t number = 0;
        std::int32_t extra_balls = 0;
        std::int32_t relaunch = 0;
    } ball;
    struct AwardsProgress {
        std::int32_t skillshot_value = 0;
        std::int32_t extra_ball_awards = 0;
        std::int32_t extra_balls_lit = 0;
        std::int32_t explosion_ready = 0;
        std::int32_t ramp_value = 0;
    } awards;
    struct RecordsProgress {
        std::int32_t replay_awarded = 0;
        std::int32_t buyins = 0;
        std::int32_t processed = 0;
        std::array<std::int32_t, 3> saved_initials{'A', 'A', 'A'};
        std::optional<ScoreValue> pre_buyin;
        bool pre_buyin_processed = false;
        std::optional<std::array<std::uint8_t, 3>> confirmed_initials;
    } records;
    struct BonusProgress {
        std::int32_t maxed = 0;
        std::int32_t sphere = 0;
        std::int32_t power_levels = 0;
        std::int32_t loops = 0;
        std::int32_t combos = 0;
        std::int32_t hold = 0;
        std::int32_t multiplier = 0;
    } bonus;
    struct MissionsProgress {
        std::int32_t sphere = 0;
        std::int32_t selected = 0;
        std::int32_t all_played = 0;
        std::int32_t rounds = 0;
        std::array<std::int32_t, 6> played{};
    } missions;
    struct CombosProgress {
        std::int32_t sphere = 0;
        std::int32_t best = 0;
        std::int32_t remaining = 0;
        std::int32_t rounds = 0;
        std::int32_t state = 0;
    } combos;
    struct StationProgress {
        std::int32_t sphere = 0;
        std::int32_t state = 0;
        std::int32_t captures = 0;
    } station;
    struct PowerProgress {
        std::int32_t sphere = 0;
        std::int32_t level = 0;
    } power;
    struct KickbackProgress {
        std::int32_t first_use_voice = 0;
        std::int32_t available = 0;
    } kickback;
    struct CompatibilityProgress {
        std::int32_t reset_word_a = 0;
        std::int32_t reset_word_b = 0;
        std::int32_t initialization_word = 0;
        std::int32_t retained_word = 0;
    } compatibility;
    struct LoopsProgress {
        std::int32_t extra_ball_threshold = 0;
        std::int32_t best = 0;
    } loops;
    struct ShowdownProgress {
        std::int32_t spheres = 0;
        std::int32_t rounds = 0;
    } showdown;
    struct LauncherProgress {
        std::int32_t state = 0;
        std::int32_t rounds = 0;
    } launcher;
    struct MagnetProgress {
        std::int32_t ready = 0;
        std::int32_t hold = 0;
    } magnet;
    struct LockProgress {
        std::int32_t state = 0;
        std::int32_t balls = 0;
        std::int32_t progress = 0;
        std::int32_t rounds = 0;
    } lock;
    struct ArenaProgress {
        std::int32_t ramps = 0;
        std::int32_t extra_ball_at = 0;
        std::int32_t lock_at = 0;
    } arena;
    struct QuickShotProgress {
        std::int32_t state = 0;
        std::int32_t count = 0;
    } quick_shot;
    struct VideoProgress {
        std::int32_t perfect_rounds = 0;
        std::int32_t ready = 0;
        std::int32_t rounds = 0;
        std::int32_t extra_ball_cycles = 0;
    } video;
    struct BumpersProgress {
        std::int32_t super_jets = 0;
        std::int32_t hold = 0;
    } bumpers;
    std::array<std::int32_t, 6> targets{};
    std::array<std::int32_t, 3> rollovers{};

private:
    PlayerId id_;
};
class PlayerRoster {
public:
    static constexpr unsigned capacity = 4;
    PlayerRoster() :
        players_{Player{PlayerId{0}}, Player{PlayerId{1}}, Player{PlayerId{2}}, Player{PlayerId{3}}} {}
    Player& at(unsigned index) {
        return players_.at(index);
    }
    const Player& at(unsigned index) const {
        return players_.at(index);
    }
    auto begin() {
        return players_.begin();
    }
    auto end() {
        return players_.end();
    }
    auto begin() const {
        return players_.begin();
    }
    auto end() const {
        return players_.end();
    }
    void reset_record_eligibility() {
        for (auto& player : players_) {
            for (unsigned i = 0; i < 3; ++i)
                if (player.id().slot || i)
                    player.records.saved_initials[i] = 'A';
            player.records.processed = 0;
            player.records.pre_buyin.reset();
            player.records.pre_buyin_processed = false;
            player.records.confirmed_initials.reset();
        }
    }

private:
    std::array<Player, capacity> players_;
};
} // namespace web::rules
