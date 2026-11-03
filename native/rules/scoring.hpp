#pragma once
#include "session_rules.hpp"
#include "../sound.hpp"
#include <functional>

namespace web::rules {
struct ReplayState {
    // Original initialized globals 0x42c88c/90/94 and 0x42c4f4.
    std::uint32_t threshold_units = 50000000;
    std::int32_t threshold_rollovers = 0, credits = 0, awards = 0;
};
enum class ScoringPage : unsigned {
    award = 33,
    flashing_award = 34,
    orbit_award = 35,
    hideout_award = 37,
    ramp_value = 41,
    superjet_total = 45,
    skillshot = 60
};
class ScoringEffects {
public:
    virtual ~ScoringEffects() = default;
    virtual bool scoring_award_page_active() const = 0;
    virtual void scoring_animation(unsigned id, std::uint32_t flags) = 0;
    virtual void scoring_page(ScoringPage page, unsigned ticks, int priority, bool replace) = 0;
    virtual void scoring_amount(ScoreValue amount) = 0;
    virtual void scoring_skillshot(std::uint32_t value) = 0;
};
class ScoringRules {
public:
    ScoringRules(TurnState& rules, SharedGameState& session, ScoringEffects* effects = nullptr) :
        rules_(rules),
        session_(session),
        effects_(effects) {}
    void add(std::uint32_t units);               // 0x40556c
    void tenths(std::uint32_t units);            // 0x40566c, unsigned division
    void hundreds(std::uint32_t units);          // 0x405680, wrapped multiplication
    void hundred_thousands(std::uint32_t units); // 0x405698
    void spinner(unsigned index = 0);            // 0x40dfe4, both spinner crossings
    void slings();                               // inline 0x40f79c..0x40f7cb
    void bumpers();                              // 0x407b38, points/accumulator only
    void ramp();                                 // 0x407898, skillshot and ramp accumulator
    void advance_bonus();                        // 0x407cb0
    void fill_next_rollover();                   // 0x407f60, kickback rearm with rescue available
    ScoreValue score() const;
    void sound_sink(SoundSink* sink) {
        sounds_ = sink;
    }
    SoundSink* sound_sink() const {
        return sounds_;
    }
    void sound(SoundRequest request) {
        if (sounds_)
            sounds_->sound(request);
    }
    void stop_sounds(int id = -1, int channel = -1, int group = -1) {
        if (sounds_)
            sounds_->stop(id, channel, group);
    }
    void announce(); // 0x406dbc, shared eight-voice selection.
    // Logical music state is retained with no optional playback backend.
    void music_track(unsigned);
    void stop_music();
    void music_resume_handler(std::function<void()> handler) {
        resume_music_ = std::move(handler);
    }
    void resume_music() {
        if (resume_music_)
            resume_music_();
    }
    ReplayState& replay() {
        return replay_;
    }
    const ReplayState& replay() const {
        return replay_;
    }

private:
    TurnState& rules_;
    SharedGameState& session_;
    ReplayState replay_;
    ScoringEffects* effects_;
    SoundSink* sounds_ = nullptr;
    std::function<void()> resume_music_;
    Player& player();
    void animation(unsigned id, std::uint32_t flags = 1);
    void page(ScoringPage kind, unsigned ticks, int priority, bool replace);
    bool show_contact_page() const;
};
} // namespace web::rules
