#pragma once
#include "dmd_raster.hpp"
#include <optional>

namespace web {
enum class PresentationScaling {
    fit,
    integer
};
struct PresentationRect {
    int x = 0, y = 0, width = 0, height = 0;
    bool operator==(const PresentationRect&) const = default;
};
struct PresentedDmd {
    int x, y, pitch, dot_width, dot_height;
};
PresentationRect presentation_viewport(int source_width, int source_height, int client_width, int client_height, PresentationScaling);
bool integer_presentation(const PresentationRect&, int source_width, int source_height);
std::optional<PresentedDmd> presented_dmd(const PresentationRect&, int source_width, int source_height, DmdLayout);
// Samples the already-rendered native dots, retaining the original colors,
// low-detail fields and displayed frame. No game/DMD controller is advanced.
void draw_presented_dmd(std::span<std::uint32_t> target, int width, int height, const Bitmap& native, DmdLayout, PresentedDmd);
} // namespace web
