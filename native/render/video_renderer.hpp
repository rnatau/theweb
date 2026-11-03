#pragma once
#include "dot_matrix.hpp"
#include "../rules/video_mode.hpp"

namespace web {
// Original 0x414990..0x414bed blits the separate video atlases. The game rules
// own objects, shots and awards; this class only draws their logical DMD pixels.
class VideoRenderer {
public:
    VideoRenderer(const DotMatrixRenderer& text, const std::filesystem::path& assets);
    DotMatrixFrame scene(const rules::VideoModeState&, rules::ScoreValue score, const DotMatrixFrame& backdrop) const;
    DotMatrixFrame page(rules::VideoPage, std::uint32_t total, bool perfect, unsigned remaining, std::uint32_t counter, const DotMatrixFrame& backdrop) const;

private:
    const DotMatrixRenderer& text_;
    Bytes sprites_, lasers_, icons_;
    static void blit(DotMatrixFrame&, std::span<const std::uint8_t> atlas, std::size_t offset, int width, int height, int stride, int x, int y);
    void object(DotMatrixFrame&, const rules::VideoObject&) const;
};
} // namespace web
