#include "video_renderer.hpp"
#include "../physics/integer.hpp"
#include <stdexcept>

namespace web {
namespace {
constexpr int object_frame_side = 32, object_atlas_stride = 256;
constexpr std::size_t object_bank_bytes = object_frame_side * object_atlas_stride;
constexpr unsigned object_bank_count = 6;
constexpr int icon_width = 8, icon_atlas_stride = 16, maximum_life_icons = 4;
} // namespace
VideoRenderer::VideoRenderer(const DotMatrixRenderer& text, const std::filesystem::path& assets) :
    text_(text),
    sprites_(read_file(assets / "MATRIX/VIDEO/SPRITES.BIT")),
    lasers_(read_file(assets / "MATRIX/VIDEO/LASERS.BIT")),
    icons_(read_file(assets / "MATRIX/VIDEO/ICONS.BIT")) {
    if (sprites_.size() != object_bank_count * object_bank_bytes || lasers_.size() != 2 * DotMatrixFrame::pixel_count || icons_.size() != icon_atlas_stride * 24)
        throw std::runtime_error("Invalid original video-mode atlas length");
}
void VideoRenderer::blit(DotMatrixFrame& frame, std::span<const std::uint8_t> atlas, std::size_t offset, int width, int height, int stride, int x, int y) {
    if (offset + std::size_t(height - 1) * stride + width > atlas.size())
        throw std::out_of_range("Original video atlas rectangle");
    for (int row = 0; row < height; ++row)
        for (int column = 0; column < width; ++column) {
            const auto value = atlas[offset + std::size_t(row) * stride + column];
            const auto dx = x + column, dy = y + row;
            if (value < 4 && dx >= 0 && dx < DotMatrixFrame::width && dy >= 0 && dy < DotMatrixFrame::height)
                frame.pixels[std::size_t(dy) * DotMatrixFrame::width + dx] = value;
        }
}
void VideoRenderer::object(DotMatrixFrame& frame, const rules::VideoObject& object) const {
    constexpr int positions[] = {16, 18, 20, 22, 24, 26, 28, 32};
    constexpr std::size_t offsets[] = {0, 0, object_bank_bytes, 2 * object_bank_bytes, 3 * object_bank_bytes, 4 * object_bank_bytes, 5 * object_bank_bytes, 4 * object_bank_bytes};
    if (object.type < rules::video_object::mine || object.type > rules::video_object::explosion_last || object.depth < 0 || object.depth >= rules::video_geometry::depth_steps || object.lane < 0 ||
        object.lane >= rules::video_geometry::lanes)
        throw std::out_of_range("Original video object rendering domain");
    const auto distance = positions[object.depth];
    const int x = object.lane == 0 ? DotMatrixFrame::width / 2 - distance : object.lane == 1 ? distance : object.lane == 2 ? DotMatrixFrame::width / 2 + distance : DotMatrixFrame::width - distance;
    blit(
        frame,
        sprites_,
        offsets[object.type] + std::size_t(rules::video_geometry::depth_steps - 1 - object.depth) * object_frame_side,
        object_frame_side,
        object_frame_side,
        object_atlas_stride,
        x - object_frame_side / 2,
        0);
}
DotMatrixFrame VideoRenderer::scene(const rules::VideoModeState& state, rules::ScoreValue amount, const DotMatrixFrame& backdrop) const {
    auto frame = backdrop;
    for (int depth = 0; depth < rules::video_geometry::depth_steps; ++depth)
        for (const auto& item : state.objects)
            if (item.type && item.depth == depth)
                object(frame, item);
    if (state.misses > maximum_life_icons)
        throw std::out_of_range("Original video life-icon rendering domain");
    for (int i = 0; i < state.misses; ++i)
        blit(frame, icons_, icon_atlas_stride * 16, icon_width, icon_width, icon_atlas_stride, DotMatrixFrame::width - icon_width, 24 - icon_width * i);
    if (state.laser == 1 || state.laser == -1)
        blit(frame, lasers_, state.laser == 1 ? 0 : DotMatrixFrame::pixel_count, DotMatrixFrame::width, DotMatrixFrame::height, DotMatrixFrame::width, 0, 0);
    text_.score(frame, amount, 3, 0, 25, DotMatrixRenderer::Alignment::centre);
    if (state.damaged)
        for (auto& value : frame.pixels)
            value ^= 3;
    return frame;
}
DotMatrixFrame VideoRenderer::page(rules::VideoPage page, std::uint32_t total, bool perfect, unsigned remaining, std::uint32_t counter, const DotMatrixFrame& backdrop) const {
    auto frame = backdrop;
    if (page == rules::VideoPage::instructions) {
        text_.text(frame, "VIDEO MODE", 5, 2, 2);
        text_.text(frame, "USE FLIPPERS TO SHOOT MINES", 3, 2, 16);
        text_.text(frame, "AVOID SHOOTING AWARDS", 3, 2, 24);
    } else {
        const auto signed_total = physics::integer::bits(total);
        const rules::ScoreValue amount{std::uint32_t(physics::integer::mul(signed_total, 100000) % std::int32_t(rules::ScoreValue::radix)), std::uint32_t((signed_total / 1000) % 10)};
        if (!perfect) {
            text_.text(frame, "VIDEO TOTAL", 5, 22, 2);
            text_.score(frame, amount, 2, 0, 16, DotMatrixRenderer::Alignment::centre);
        } else if (remaining > 24)
            text_.text(frame, "PERFECT!!!", 5, 24, 8);
        else if ((counter + 1) & 1)
            text_.score(frame, amount, 4, 0, 0, DotMatrixRenderer::Alignment::centre);
    }
    return frame;
}
} // namespace web
