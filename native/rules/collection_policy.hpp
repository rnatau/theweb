#pragma once
#include "session_rules.hpp"

namespace web::rules {
// Optional host-supplied permission for collecting a new mode. Existing mode
// objectives, jackpots and qualification progress have their own controllers.
using CollectionPolicy = bool (*)(const TurnState&, const SharedGameState&);
} // namespace web::rules
