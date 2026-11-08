#include "../render/sprite_coverage.hpp"
#include "../render/table_renderer.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void check(bool condition, const char* message) {
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
web::Bitmap canvas() {
    web::Bitmap result{16, 16, {}};
    result.pixels.assign(256, 0x204080);
    result.palette[7] = 0x80c020;
    return result;
}
} // namespace
int main() {
    try {
        using namespace web;
        std::vector<Span> spans;
        for (int y = 3; y <= 10; ++y)
            spans.push_back({3, y, std::vector<std::uint16_t>(unsigned(y - 1), 0x7fff)});
        const SpriteCoverage sprite(spans);
        check(std::any_of(sprite.pixels().begin(), sprite.pixels().end(), [](const auto& pixel) { return pixel.coverage < SpriteCoverage::samples; }), "Diagonal mask did not gain partial coverage");
        TileMap depth;
        depth.columns = depth.rows = 2;
        depth.cells.assign(4, 0);
        depth.tiles.assign(64, 255);
        auto image = canvas();
        sprite.composite(image, depth, 0, 0, 0, 2);
        std::vector<bool> support(image.pixels.size());
        for (const auto& pixel : sprite.pixels()) {
            check(pixel.coverage > 0 && pixel.coverage <= SpriteCoverage::samples, "Invalid sprite coverage");
            const auto index = std::size_t(pixel.y) * image.width + pixel.x;
            support.at(index) = true;
            const auto background = canvas().pixels[index];
            for (const unsigned shift : {0u, 8u, 16u}) {
                const auto expected = (255 * pixel.coverage + ((background >> shift) & 255) * (SpriteCoverage::samples - pixel.coverage) + SpriteCoverage::samples / 2) / SpriteCoverage::samples;
                check(((image.pixels[index] >> shift) & 255) == expected, "Silhouette blend changed sprite color or used transparent black");
            }
        }
        for (unsigned pixel = 0; pixel < image.pixels.size(); ++pixel)
            if (!support[pixel])
                check(image.pixels[pixel] == canvas().pixels[pixel], "Sprite antialiasing changed unrelated artwork");
        const auto expected = image;
        const std::uint8_t ball_depth = 1;
        depth.tiles.assign(64, 0);
        image = canvas();
        sprite.composite(image, depth, 0, 0, 0, 2, &ball_depth);
        check(image.pixels == canvas().pixels, "Sprite fringe bypassed table occlusion");
        depth.tiles.assign(64, 255);
        sprite.composite(image, depth, 0, 0, 0, 2, &ball_depth);
        check(image.pixels == expected.pixels, "Visible sprite changed under an open depth gate");
        // One sprite crosses a foreground table edge. Visibility is decided
        // independently for every opaque and partial-coverage destination.
        depth.cells = {0, 1, 0, 1};
        depth.tiles.assign(128, ball_depth);
        std::fill_n(depth.tiles.begin(), 64, 0);
        image = canvas();
        sprite.composite(image, depth, 0, 0, 0, 2, &ball_depth);
        unsigned hidden_fringe = 0, visible_fringe = 0;
        for (const auto& pixel : sprite.pixels())
            if (pixel.coverage < SpriteCoverage::samples)
                pixel.x < 8 ? ++hidden_fringe : ++visible_fringe;
        check(hidden_fringe > 0 && visible_fringe > 0, "Partial occlusion fixture did not cross both sprite edge regions");
        for (int y = 0; y < image.height; ++y)
            for (int x = 0; x < image.width; ++x) {
                const auto index = std::size_t(y) * image.width + x;
                check(image.pixels[index] == (x < 8 ? canvas().pixels[index] : expected.pixels[index]), "Partial occlusion leaked the hidden sprite or changed its visible antialiased edge");
            }
        // Identical mask contours must intersect once, rather than square
        // their opacities. Complementary contours must have no shared samples.
        depth.cells = {0, 1, 2, 3};
        depth.tiles.assign(256, 0);
        const auto set_visibility = [&](int x, int y, bool visible) {
            const auto tile = unsigned((y / 8) * 2 + x / 8);
            depth.tiles[tile * 64 + (y & 7) * 8 + (x & 7)] = visible ? ball_depth : 0;
        };
        for (const auto& span : spans)
            for (unsigned x = 0; x < span.pixels.size(); ++x)
                set_visibility(span.x + int(x), span.y, true);
        image = canvas();
        sprite.composite(image, depth, 0, 0, 0, 2, &ball_depth);
        check(image.pixels == expected.pixels, "Matching sprite/depth contours were blended twice");
        for (auto& value : depth.tiles)
            value = value ? 0 : ball_depth;
        image = canvas();
        sprite.composite(image, depth, 0, 0, 0, 2, &ball_depth);
        check(image.pixels == canvas().pixels, "Complementary sprite/depth contours leaked ball color");
        std::vector<Span> square;
        for (int y = 2; y < 14; ++y)
            square.push_back({2, y, std::vector<std::uint16_t>(12, 0x7fff)});
        const SpriteCoverage square_sprite(square);
        auto square_open = canvas();
        square_sprite.composite(square_open, depth, 0, 0, 0, 2);
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
                set_visibility(x, y, x >= y);
        image = canvas();
        square_sprite.composite(image, depth, 0, 0, 0, 2, &ball_depth);
        auto hard = canvas();
        TableRenderer::composite(hard, depth, square, 0, 0, 0, 2, &ball_depth);
        unsigned softened_occluder = 0;
        for (const auto& pixel : square_sprite.pixels()) {
            const auto index = std::size_t(pixel.y) * image.width + pixel.x;
            if (pixel.coverage == SpriteCoverage::samples && image.pixels[index] != hard.pixels[index]) {
                check(image.pixels[index] != 0xffffff && image.pixels[index] != canvas().pixels[index], "Occluder contour was not given partial coverage");
                ++softened_occluder;
            }
            if (std::abs(pixel.x - pixel.y) > 1)
                check(image.pixels[index] == (pixel.x > pixel.y ? square_open.pixels[index] : canvas().pixels[index]), "Depth antialiasing changed a fully hidden or visible interior");
        }
        check(softened_occluder > 0, "Diagonal occluder retained a binary staircase inside the sprite");
        image = canvas();
        sprite.composite(image, depth, -32, -32, 0, 2);
        check(image.pixels == canvas().pixels, "Off-canvas sprite wrapped into the table");
        sprite.composite(image, depth, -4, -4, 0, 2);
        check(image.pixels != canvas().pixels, "Clipped sprite was discarded entirely");
        SpriteCoverage empty({});
        check(empty.pixels().empty(), "Empty sprite invented coverage");
        std::vector<Span> rectangle;
        for (int y = 3; y < 10; ++y)
            rectangle.push_back({3, y, std::vector<std::uint16_t>(7, 7)});
        SpriteCoverage indexed(rectangle);
        image = canvas();
        indexed.composite(image, depth, 0, 0, 0, 1);
        for (int y = 4; y < 9; ++y) {
            check(image.pixels[y * 16 + 3] == image.palette[7], "Straight aligned sprite edge was blurred");
            check(image.pixels[y * 16 + 9] == image.palette[7], "Opposite aligned sprite edge was blurred");
        }
        std::cout << "PASS: " << checks << " silhouette coverage/color/occlusion/clipping checks\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
