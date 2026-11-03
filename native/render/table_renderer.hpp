#pragma once
#include "table_state.hpp"
#include "indexed_shadow.hpp"
#include "ball_presentation.hpp"
#include "lamp_presentation.hpp"
#include "ball_draw_order.hpp"
#include <optional>

namespace web {
struct DotMatrixFrame;
struct ProjectedBall {
    int x = 0, y = 0, frame = 0;
    std::uint8_t depth = 0;
    bool operator==(const ProjectedBall&) const = default;
};
// Original integer perspective projection 0x402d10; immutable camera records
// captured from 0x40132c with the original initialized x87 constants.
class BallProjection {
public:
    explicit BallProjection(unsigned variant);
    ProjectedBall project(const physics::BallState& ball) const;

private:
    struct Camera {
        std::int32_t pitch_sine, pitch_cosine;
        std::int32_t screen_y, focal_scale, world_y, world_z, world_x, screen_x;
        std::int32_t depth_far, depth_near, size_y_end, size_y_start, size_frame_scale;
    } camera_{};
};
class TableRenderer {
public:
    void draw_display(Bitmap&, const DotMatrixFrame&, bool low_detail);
    TableRenderer(const std::filesystem::path& assets, unsigned variant);
    const Bitmap&
    compose(TableMotionView simulation, bool depth = false, std::span<const std::int32_t> lamps = {}, const DotMatrixFrame* display = nullptr, bool advance_raster = true, bool current_motion = false);
    const Bitmap& compose(TableFrameView frame, unsigned detail, bool depth = false) {
        graphic_detail(detail, frame.live_balls);
        return compose(frame.motion, depth, frame.lamps, &frame.display);
    }
    const Bitmap& compose_current(TableFrameView frame, unsigned detail, bool depth = false) {
        graphic_detail(detail, frame.live_balls);
        return compose(frame.motion, depth, frame.lamps, &frame.display, false, true);
    }
    void advance_raster() {
        ++raster_counter_;
    }
    // Advance visible lamps once per presentation frame, not on pause/repaint.
    void enable_lamp_timing(bool flipping = true);
    void advance_lamps(std::span<const std::int32_t, 79> lamps, std::int32_t repetitions, bool animation, bool force_refresh);
    void advance_balls(TableMotionView simulation);
    unsigned variant() const {
        return variant_;
    }
    void graphic_detail(unsigned detail, std::int32_t live_balls) {
        ball_policy_ = BallPresentationPolicy(detail);
        live_balls_ = live_balls;
    }
    std::uint32_t raster_counter() const {
        return raster_counter_;
    }
    // Artwork under the in-table display, captured before native dot rastering.
    // Presentation can rescale the artwork and place crisp dots independently.
    const Bitmap& display_backdrop() const {
        return display_backdrop_;
    }
    static unsigned flipper_frame(const physics::FlipperState& flipper);
    static unsigned spinner_frame(const physics::SpinnerState& spinner);
    static void composite(Bitmap& screen, const TileMap& depth, std::span<const Span> spans, int x, int y, int top, int bpp, const std::uint8_t* ball_depth = nullptr, bool shadow = false);
    static void composite(
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
        const Bitmap* shadow_backdrop = nullptr);

private:
    unsigned variant_;
    int bpp_, depth_top_;
    Bitmap background_, lamp_backdrop_, screen_;
    Bitmap display_backdrop_;
    TileMap depth_;
    TileMap base_depth_;
    std::optional<TileMap> translated_depth_;
    BallProjection projection_;
    std::optional<IndexedShadow> indexed_shadow_;
    BallPresentationPolicy ball_policy_;
    std::int32_t live_balls_ = 1;
    std::uint32_t raster_counter_ = 0;
    LampPresentation lamp_presentation_;
    std::array<std::int32_t, 79> lamp_costs_{};
    bool timed_lamps_ = false;
    bool lamp_flipping_ = true;
    unsigned lamp_page_ = 0, next_lamp_page_ = 0;
    std::array<Bitmap, 2> lamp_pages_;
    BallDrawOrder ball_order_;
    std::array<unsigned, 6> ball_slots_{0, 1, 2, 3, 4, 5};
    std::array<ProjectedBall, 6> projected_balls_{};
    bool projected_frame_ = false;
    using SpriteFrames = std::vector<std::vector<Span>>;
    struct MechanismArtwork {
        SpriteFrames frames;
        Container depth_patches;
    };
    // Each mechanism keeps its visible frames and occlusion patches together.
    std::array<MechanismArtwork, 3> flippers_;
    std::array<MechanismArtwork, 2> spinners_;
    SpriteFrames ball_, shadow_;
    std::array<SpriteFrames, 3> lamps_; // Original controller banks: off, on, half.
    std::array<int, 3> flipper_offsets_{};
    void translate_flippers(TableMotionView simulation);
    void sprite(const SpriteFrames& bank, unsigned frame, int x, int y, const std::uint8_t* depth = nullptr, bool shadow = false);
};
} // namespace web
