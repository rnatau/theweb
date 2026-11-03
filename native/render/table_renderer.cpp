#include "table_renderer.hpp"
#include "dmd_raster.hpp"
#include "../physics/integer.hpp"
#include <algorithm>
#include <stdexcept>

namespace web {
using namespace physics::integer;
namespace {
std::uint32_t color555(std::uint16_t v) {
    return (((v >> 10 & 31) * 255 + 15) / 31) << 16 | (((v >> 5 & 31) * 255 + 15) / 31) << 8 | ((v & 31) * 255 + 15) / 31;
}
std::uint32_t shade(std::uint32_t color) {
    // Invert the nearest-rounded RGB555 expansion, apply the original quarter
    // subtraction, then expand again using the same table/sprite conversion.
    const auto exact = [](unsigned v) {
        auto c = ((v & 255) * 31 + 127) / 255;
        return ((c - (c >> 2)) * 255 + 15) / 31;
    };
    return exact(color >> 16) << 16 | exact(color >> 8) << 8 | exact(color);
}
} // namespace
BallProjection::BallProjection(unsigned variant) {
    static constexpr std::int32_t records[][19] = {
#include "camera_defaults.inc"
    };
    for (const auto& record : records)
        if (unsigned(record[0]) == variant) {
            // Zero-based slots 2..6 of the 18-word camera are not used by projection.
            // Decode this binary record once; the equations below use roles.
            camera_ = {record[1], record[2], record[8], record[9], record[10], record[11], record[12], record[13], record[14], record[15], record[16], record[17], record[18]};
            return;
        }
    throw std::invalid_argument("Unsupported projection view");
}
ProjectedBall BallProjection::project(const physics::BallState& ball) const {
    const auto& c = camera_;
    const auto position = ball.position();
    constexpr int projection_scale = 64, world_scale_divisor = 0x534;
    constexpr int world_origin_x = 153 * physics::fixed_point::position_one, world_origin_y = 307 * physics::fixed_point::position_one;
    constexpr int ball_z_offset = 7 * physics::fixed_point::position_one, surface_z_offset = 0x5666;
    const auto x = mul(sub(position.x, world_origin_x), projection_scale) / world_scale_divisor;
    const auto y = mul(sub(position.y, world_origin_y), projection_scale) / world_scale_divisor;
    auto z = mul(add(ball.z(), ball_z_offset), projection_scale) / world_scale_divisor;
    if (ball.motion_mode() == physics::motion_mode::table)
        z = add(ball.surface_height() / 4, surface_z_offset);
    if (ball.motion_mode() == physics::motion_mode::falling)
        z = add(ball.falling_height() / 4, surface_z_offset);
    const auto cy = sub(y, c.world_y), cz = sub(z, c.world_z);
    const auto vertical = bits(std::uint32_t((std::int64_t(c.pitch_cosine) * cz - std::int64_t(c.pitch_sine) * cy) >> 15));
    const auto distance = bits(std::uint32_t((std::int64_t(c.pitch_sine) * cz + std::int64_t(c.pitch_cosine) * cy) >> 15));
    const auto dx = mul(distance, 32) / c.focal_scale, dy = mul(distance, 64) / c.focal_scale;
    if (dx == 0 || dy == 0)
        throw std::out_of_range("Ball crosses original projection plane");
    const auto q = sub(x, c.world_x) / dx;
    const auto half = add(q, q < 0 ? -1 : 1) / 2;
    auto frame = std::clamp(mul(sub(mul(y / 16384, 65536), c.size_y_start), c.size_frame_scale) / sub(c.size_y_end, c.size_y_start), 5, 63);
    if (half < 0 ? (q & 1) != 0 : (q & 1) == 0)
        frame += 64;
    return {add(c.screen_x, half), sub(c.screen_y, vertical / dy), frame, std::uint8_t(sub(mul(distance, 4), mul(c.depth_near, 256)) / sub(c.depth_far, c.depth_near))};
}
TableRenderer::TableRenderer(const std::filesystem::path& assets, unsigned variant) :
    variant_(variant),
    bpp_(display_mode(variant).bytes_per_pixel),
    background_(load_table(assets, variant)),
    lamp_backdrop_(background_),
    screen_(background_),
    depth_(TileMap::load(assets / variant_name(variant) / "TABLE/VIEW.MAP", background_.width / 8)),
    base_depth_(depth_),
    projection_(variant) {
    if (bpp_ == 1)
        indexed_shadow_.emplace(background_.palette);
    // Only depth-map rows omit the backbox band. SPR headers and projected
    // balls already use absolute table-canvas coordinates (0x419096).
    depth_top_ = table_depth_top(variant);
    const auto directory = assets / variant_name(variant);
    const auto load_frames = [&](const char* name, int pixel_bytes, std::span<std::int32_t> costs = {}) {
        const auto bank = Container::load(directory / "SPRITES" / (std::string(name) + ".SPR"));
        for (std::size_t frame = 0; frame < costs.size(); ++frame)
            costs[frame] = std::int32_t(bank.block(frame).size());
        SpriteFrames frames;
        frames.reserve(bank.size());
        for (std::size_t frame = 0; frame < bank.size(); ++frame)
            frames.push_back(decode_sprite(bank, frame, pixel_bytes));
        return frames;
    };
    const auto load_mechanism = [&](const char* sprite, const char* patch) {
        return MechanismArtwork{load_frames(sprite, bpp_), Container::load(directory / "TABLE" / (std::string(patch) + ".ZMP"), 16)};
    };
    flippers_ = {load_mechanism("FLIPL", "FLIPL"), load_mechanism("FLIPR", "FLIPR"), load_mechanism("FLIPU", "FLIPU")};
    spinners_ = {load_mechanism("SPINNERA", "SPINA"), load_mechanism("SPINNERB", "SPINB")};
    ball_ = load_frames("BALL", bpp_);
    shadow_ = load_frames("BALLSHAD", 0);
    lamps_ = {load_frames("LIGHTOFF", bpp_, lamp_costs_), load_frames("LIGHTON", bpp_), load_frames("LIGHTHLF", bpp_)};
}
void TableRenderer::enable_lamp_timing(bool flipping) {
    timed_lamps_ = true;
    lamp_flipping_ = flipping;
    lamp_page_ = next_lamp_page_ = 0;
    lamp_presentation_ = LampPresentation{};
    lamp_backdrop_ = background_;
    lamp_pages_.fill(background_);
}
void TableRenderer::advance_lamps(std::span<const std::int32_t, 79> lamps, std::int32_t repetitions, bool animation, bool force_refresh) {
    if (!timed_lamps_)
        enable_lamp_timing();
    if (force_refresh)
        lamp_presentation_.force_refresh();
    lamp_page_ = next_lamp_page_;
    next_lamp_page_ ^= unsigned(lamp_flipping_);
    const auto draws = lamp_presentation_.step(lamps, lamp_costs_, {variant_, repetitions, animation, lamp_flipping_, lamp_page_});
    for (const auto& draw : draws) {
        const auto& spans = lamps_.at(draw.bank).at(draw.lamp);
        if (lamp_page_ == 0)
            composite(lamp_backdrop_, depth_, spans, 0, 0, depth_top_, bpp_);
        composite(lamp_pages_[lamp_page_], depth_, spans, 0, 0, depth_top_, bpp_);
    }
}
void TableRenderer::advance_balls(TableMotionView simulation) {
    BallDrawOrder::Coordinates current_y;
    for (unsigned slot = 0; slot < projected_balls_.size(); ++slot) {
        projected_balls_[slot] = projection_.project(simulation.balls[slot]);
        current_y[slot] = projected_balls_[slot].y;
    }
    ball_slots_ = ball_order_.step(current_y);
    projected_frame_ = true;
}
unsigned TableRenderer::flipper_frame(const physics::FlipperState& flipper) {

    auto angle = sub(flipper.angle, flipper.rest_angle);
    if (flipper.direction != 1)
        angle = sub(0, angle);
    return unsigned(std::clamp(mul(angle, 12) / 0x2aaa, 0, 11));
}
unsigned TableRenderer::spinner_frame(const physics::SpinnerState& spinner) {
    // Original display selector 0x40434e; physics phase is one frame ahead.
    return unsigned(sub(spinner.phase, 0x8000) >> 15) & 15;
}
void TableRenderer::sprite(const SpriteFrames& bank, unsigned frame, int x, int y, const std::uint8_t* ball_depth, bool shadow) {
    composite(
        screen_,
        translated_depth_ ? *translated_depth_ : depth_,
        bank.at(frame),
        x,
        y,
        depth_top_,
        bpp_,
        ball_depth,
        shadow,
        indexed_shadow_ ? &*indexed_shadow_ : nullptr,
        shadow ? &lamp_backdrop_ : nullptr);
}
void TableRenderer::translate_flippers(TableMotionView simulation) {

    static const auto original = physics::MechanismState::defaults();
    bool moved = false;
    for (unsigned i = 0; i < flipper_offsets_.size(); ++i) {
        const auto& before = original.flippers[i];
        const auto& after = simulation.mechanisms.flippers[i];
        physics::BallState point;
        point.position({before.pivot_x * 256, before.pivot_y * 256});
        const auto source = projection_.project(point);
        point.position({after.pivot_x * 256, before.pivot_y * 256});
        flipper_offsets_[i] = projection_.project(point).x - source.x;
        moved |= flipper_offsets_[i] != 0;
    }
    if (!moved) {
        translated_depth_.reset();
        return;
    }
    // Keep the recovered ZMP bank intact. Clone only modified depth tiles so
    // pixel-sized translations move ball occlusion with the visible sprites.
    translated_depth_ = depth_;
    auto& target = *translated_depth_;
    std::vector<bool> cloned(target.cells.size());
    const auto write = [&](int x, int y, std::uint8_t value) {
        if (x < 0 || y < 0 || x >= target.columns * 8 || y >= target.rows * 8)
            return;
        const auto cell = std::size_t(y / 8) * target.columns + x / 8;
        auto& tile = target.cells[cell];
        if (!cloned[cell]) {
            if (target.tiles.size() / 64 > 65535)
                throw std::out_of_range("Translated depth tile bank is full");
            std::array<std::uint8_t, 64> pixels;
            std::copy_n(target.tiles.begin() + std::size_t(tile) * 64, 64, pixels.begin());
            tile = std::uint16_t(target.tiles.size() / 64);
            target.tiles.insert(target.tiles.end(), pixels.begin(), pixels.end());
            cloned[cell] = true;
        }
        target.tiles[std::size_t(tile) * 64 + (y & 7) * 8 + (x & 7)] = value;
    };
    // Restore both old silhouettes before placing either translated silhouette.
    for (unsigned i = 0; i < flippers_.size(); ++i)
        if (flipper_offsets_[i])
            for (const auto& span : flippers_[i].frames.at(flipper_frame(simulation.mechanisms.flippers[i])))
                for (std::size_t pixel = 0; pixel < span.pixels.size(); ++pixel) {
                    const int x = span.x + int(pixel), y = span.y - depth_top_;
                    write(x, y, base_depth_.sample(x, y));
                }
    for (unsigned i = 0; i < flippers_.size(); ++i)
        if (flipper_offsets_[i])
            for (const auto& span : flippers_[i].frames.at(flipper_frame(simulation.mechanisms.flippers[i])))
                for (std::size_t pixel = 0; pixel < span.pixels.size(); ++pixel) {
                    const int x = span.x + int(pixel), y = span.y - depth_top_;
                    write(x + flipper_offsets_[i], y, depth_.sample(x, y));
                }
}
void TableRenderer::composite(Bitmap& screen, const TileMap& depth, std::span<const Span> spans, int x, int y, int top, int bpp, const std::uint8_t* ball_depth, bool shadow) {
    std::optional<IndexedShadow> indexed;
    if (shadow && bpp == 1)
        indexed.emplace(screen.palette);
    composite(screen, depth, spans, x, y, top, bpp, ball_depth, shadow, indexed ? &*indexed : nullptr);
}
void TableRenderer::composite(
    Bitmap& screen,
    const TileMap& depth,
    std::span<const Span> spans,
    int x,
    int y,
    int top,
    int bpp,
    const std::uint8_t* ball_depth,
    bool shadow,
    const IndexedShadow* indexed_shadow,
    const Bitmap* shadow_backdrop) {
    if (shadow && bpp == 1 && !indexed_shadow)
        throw std::invalid_argument("Indexed shadow palette was not initialized");
    for (const auto& span : spans) {
        const int row = y + span.y;
        if (row < 0 || row >= screen.height)
            continue;
        for (std::size_t i = 0; i < span.pixels.size(); i++) {
            const int col = x + span.x + int(i);
            if (col < 0 || col >= screen.width)
                continue;
            if (ball_depth && (row < top || row >= top + depth.rows * 8 || depth.sample(col, row - top) < *ball_depth))
                continue;
            auto& pixel = screen.pixels[std::size_t(row) * screen.width + col];
            const auto source = shadow_backdrop ? shadow_backdrop->pixels[std::size_t(row) * screen.width + col] : pixel;
            pixel = shadow ? (indexed_shadow ? indexed_shadow->shade(source) : shade(source)) : (bpp == 2 ? color555(span.pixels[i]) : screen.palette[span.pixels[i]]);
        }
    }
}
void TableRenderer::draw_display(Bitmap& target, const DotMatrixFrame& display, bool low_detail) {
    DmdRaster::draw(target, display, variant_, low_detail, ++raster_counter_);
}
const Bitmap& TableRenderer::compose(TableMotionView simulation, bool depth, std::span<const std::int32_t> lamps, const DotMatrixFrame* display, bool advance_raster, bool current_motion) {
    if (!lamps.empty() && lamps.size() != 79)
        throw std::invalid_argument("Expected all 79 lamp outputs");
    if (!timed_lamps_) {
        lamp_backdrop_ = background_;
        // Diagnostic one-shot composition uses settled requested lamp values.
        // Live presentation advances through the original controller instead.
        for (std::size_t i = lamps.size(); i > 0; --i)
            composite(lamp_backdrop_, depth_, lamps_.at(lamps[i - 1] == 0 ? 0 : lamps[i - 1] < 0 ? 2 : 1).at(i - 1), 0, 0, depth_top_, bpp_);
    }
    screen_ = depth ? background_ : timed_lamps_ ? lamp_pages_[lamp_page_] : lamp_backdrop_;
    for (unsigned i = 0; i < 3; i++)
        depth_.patch(flippers_[i].depth_patches, flipper_frame(simulation.mechanisms.flippers[i]));
    for (unsigned i = 0; i < 2; i++)
        depth_.patch(spinners_[i].depth_patches, spinner_frame(simulation.mechanisms.spinners[i]));
    translate_flippers(simulation);
    if (depth) {
        const auto& visible_depth = translated_depth_ ? *translated_depth_ : depth_;
        for (int y = 0; y < std::min(depth_.rows * 8, screen_.height - depth_top_); y++)
            for (int x = 0; x < screen_.width; x++)
                screen_.pixels[std::size_t(y + depth_top_) * screen_.width + x] = visible_depth.sample(x, y) * 0x010101;
        return screen_;
    }
    for (unsigned i = 0; i < 3; i++)
        sprite(flippers_[i].frames, flipper_frame(simulation.mechanisms.flippers[i]), flipper_offsets_[i], 0);
    for (unsigned i = 0; i < 2; i++)
        sprite(spinners_[i].frames, spinner_frame(simulation.mechanisms.spinners[i]), 0, 0);
    for (const auto slot : ball_slots_) {
        const auto& ball = simulation.balls[slot];
        const auto draws = ball_policy_.evaluate(ball, live_balls_);
        if (draws.ball) {
            const auto p = projected_frame_ && !current_motion ? projected_balls_[slot] : projection_.project(ball);
            if (draws.shadow)
                sprite(shadow_, unsigned(p.frame), p.x, p.y, &p.depth, true);
            sprite(ball_, unsigned(p.frame), p.x, p.y, &p.depth);
        }
    }
    if (display) {
        const auto area = DmdRaster::layout(variant_);
        display_backdrop_.width = area.width();
        display_backdrop_.height = area.height();
        display_backdrop_.pixels.resize(std::size_t(area.width()) * area.height());
        for (int row = 0; row < area.height(); ++row)
            std::copy_n(screen_.pixels.begin() + std::size_t(area.y + row) * screen_.width + area.x, area.width(), display_backdrop_.pixels.begin() + std::size_t(row) * area.width());
        if (advance_raster)
            draw_display(screen_, *display, ball_policy_.low_detail());
        else
            DmdRaster::draw(screen_, *display, variant_, ball_policy_.low_detail(), raster_counter_);
    }
    return screen_;
}
} // namespace web
