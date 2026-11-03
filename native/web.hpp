#pragma once
#include "physics/nudge.hpp"
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace web {
using Bytes = std::vector<std::uint8_t>;
Bytes read_file(const std::filesystem::path& path);
void write_file(const std::filesystem::path& path, std::span<const std::uint8_t> bytes);
std::uint16_t u16(std::span<const std::uint8_t> bytes, std::size_t offset);
std::uint32_t u32(std::span<const std::uint8_t> bytes, std::size_t offset);

// Original WGAME 0x41b85c: this initializer leaves version zero; serialization sets it to one.
using Configuration = std::array<std::uint32_t, 21>;
namespace configuration_field {
inline constexpr unsigned version = 0, screen_mode = 1, view = 2, music_restart = 3, graphic_detail = 4, screen_flip = 5;
inline constexpr unsigned first_key = 6, sound_16bit = 11, sound_stereo = 12, sound_channels = 14, sound_rate = 15;
inline constexpr unsigned music_volume = 17, effects_volume = 18, visual_nudge = 19;
} // namespace configuration_field
inline constexpr std::size_t configuration_record_bytes = 21 * sizeof(std::uint32_t);
Configuration default_configuration();
Bytes serialize_configuration(Configuration value);
Configuration parse_configuration(std::span<const std::uint8_t> bytes);
struct Scores {
    static constexpr std::size_t payload_bytes = 156, header_bytes = 8, record_bytes = header_bytes + payload_bytes;
    std::array<std::uint8_t, payload_bytes> payload{};
};
Bytes serialize_scores(const Scores& scores);
Scores parse_scores(std::span<const std::uint8_t> bytes);

struct Bitmap {
    int width = 0, height = 0;
    std::vector<std::uint32_t> pixels; // 0x00RRGGBB, top row first
    std::array<std::uint32_t, 256> palette{};
};
struct DisplayMode {
    int width, height, bytes_per_pixel;
};
DisplayMode display_mode(unsigned variant);
// Original 0x4169ab: depth rows start below a quarter-screen backbox in views
// 4/5/6. Tile-map padding is not part of the screen coordinate origin.
int table_depth_top(unsigned variant);
Bitmap load_table(const std::filesystem::path& pc_dat, unsigned variant);
// Original DirectDraw palette entries use six-bit components shifted left two;
// full source intensity 63 becomes 252, rather than normalized 255.
std::array<std::uint32_t, 256> decode_palette(std::span<const std::uint8_t> bytes);
Bitmap load_bitmap(const std::filesystem::path& path, DisplayMode mode);
void write_bmp(const std::filesystem::path& path, const Bitmap& bitmap);
std::string variant_name(unsigned variant);

// Outer containers: SPR/DMS offsets relative to zero, ZMP offsets relative to 16.
struct Container {
    Bytes bytes;
    std::vector<std::uint32_t> offsets;
    std::size_t origin = 0;
    static Container load(const std::filesystem::path& path, std::size_t origin = 0);
    std::size_t size() const {
        return offsets.empty() ? 0 : offsets.size() - 1;
    }
    std::span<const std::uint8_t> block(std::size_t frame) const;
};
struct Span {
    int x, y;
    std::vector<std::uint16_t> pixels;
};
// bytes_per_pixel=0 decodes a geometry-only shadow/restoration mask.
std::vector<Span> decode_sprite(const Container& bank, std::size_t frame, int bytes_per_pixel);
void draw_sprite(Bitmap& target, const Container& bank, std::size_t frame, int bytes_per_pixel, int x = 0, int y = 0);

struct TileMap {
    int columns = 0, rows = 0;
    std::vector<std::uint16_t> cells;
    Bytes tiles;
    static TileMap load(const std::filesystem::path& map, int columns);
    std::uint8_t sample(int x, int y) const;
    // Exact word load made by original helper 0x418d00 (can span adjacent tile bytes).
    std::uint16_t sample_word(int x, int y) const;
    void patch(const Container& zmp, std::size_t frame);
};
std::size_t validate_assets(const std::filesystem::path& pc_dat);
} // namespace web
