#pragma once
#include "../../render/presentation.hpp"

namespace web {
// Host presentation preferences, separate from the original 84-byte config.
struct WindowDisplay {
    bool fullscreen = false;
    bool explicit_mode = false;
    PresentationScaling scaling = PresentationScaling::fit;
};
} // namespace web
