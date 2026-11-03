#include "extra_balls.hpp"
#include "../physics/integer.hpp"

namespace web::rules {
namespace F = web::rules::turn_field;

void ExtraBallRules::collect_lit() {
    apply(Source::lit);
}
void ExtraBallRules::collect_diverter() {
    apply(Source::diverter);
}
void ExtraBallRules::award() {
    apply(Source::direct);
}
void ExtraBallRules::award_without_animation() {
    apply(Source::unanimated);
}
void ExtraBallRules::apply(Source source) {
    constexpr int award_flash = rule_time::second;
    auto& p = session_.players().at(std::uint32_t(rules_.current_player));
    if (sounds_) {
        const int id = source == Source::unanimated ? 98 : (source == Source::diverter && rules_.extra_ball_diverter < rule_time::second ? 62 : 36);
        sounds_->sound({id, source == Source::unanimated ? -64 : 64, 0, 1});
    }
    const auto increment = [](std::int32_t& value) { value = physics::integer::add(value, 1); };
    LampRules{}.queue(rules_, session_, TableLamp::extra_ball_award, award_flash);
    if (source == Source::lit)
        p.awards.extra_balls_lit = physics::integer::sub(p.awards.extra_balls_lit, 1);
    if (source == Source::diverter)
        rules_.extra_ball_diverter = 0;
    // Direct awards request the animation before incrementing collected balls;
    // collections request it after. Preserve the original callback-time state.
    if (source != Source::direct)
        increment(p.ball.extra_balls);
    if (source != Source::unanimated && animation_)
        animation_();
    if (source == Source::direct)
        increment(p.ball.extra_balls);
    increment(p.awards.extra_ball_awards);
    increment(p.loops.extra_ball_threshold);
}
} // namespace web::rules
