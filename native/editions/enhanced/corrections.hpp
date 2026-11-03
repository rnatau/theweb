#pragma once
#include "../../game_corrections.hpp"

namespace web::enhanced {
GameCorrections corrections();
void normalize_sound(SoundRequest&);
} // namespace web::enhanced
