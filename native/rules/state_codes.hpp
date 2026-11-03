#pragma once
#include <cstdint>

namespace web::rules {
// Original dispatcher phases. PinballGame::Phase is separate host bookkeeping;
// it groups these phases and must not replace the recovered dispatcher state.
enum class GamePhase : std::int32_t {
    attract = 0,
    begin_ball = 1,
    serve_ball = 2,
    playing = 3,
    bonus = 4,
    records = 5,
    match = 6,
    finish = 7
};
namespace dmd_status {
inline constexpr std::int32_t startup = -1, table = 0, video = 3;
}
} // namespace web::rules
