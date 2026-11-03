#include "web.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <limits>

namespace web {
static void require(bool condition, const std::string& message) {
    if (!condition)
        throw std::runtime_error(message);
}
static std::string utf8_path(const std::filesystem::path& path) {
    const auto text = path.u8string();
    return {text.begin(), text.end()};
}
static void require(bool condition, const char* message, const std::filesystem::path& path) {
    if (!condition)
        throw std::runtime_error(std::string(message) + utf8_path(path));
}
Bytes read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    require(bool(file), "Cannot open ", path);
    const auto length = file.tellg();
    require(length >= 0 && length < 256 * 1024 * 1024, "Invalid asset size");
    Bytes result(static_cast<std::size_t>(length));
    file.seekg(0);
    if (!result.empty())
        file.read(reinterpret_cast<char*>(result.data()), length);
    require(bool(file), "Cannot read ", path);
    return result;
}
void write_file(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
    std::ofstream file(path, std::ios::binary);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(bool(file), "Cannot write ", path);
}
std::uint16_t u16(std::span<const std::uint8_t> b, std::size_t o) {
    require(o <= b.size() && b.size() - o >= 2, "Truncated 16-bit field");
    return std::uint16_t(unsigned(b[o]) | unsigned(b[o + 1]) << 8);
}
std::uint32_t u32(std::span<const std::uint8_t> b, std::size_t o) {
    require(o <= b.size() && b.size() - o >= 4, "Truncated 32-bit field");
    return std::uint32_t(u16(b, o)) | std::uint32_t(u16(b, o + 2)) << 16;
}
static void put16(Bytes& b, std::size_t o, std::uint16_t v) {
    b.at(o) = std::uint8_t(v);
    b.at(o + 1) = std::uint8_t(v >> 8);
}
static void put32(Bytes& b, std::size_t o, std::uint32_t v) {
    put16(b, o, std::uint16_t(v));
    put16(b, o + 2, std::uint16_t(v >> 16));
}
Configuration default_configuration() {
    Configuration value{};
    value[configuration_field::screen_mode] = 1;
    value[configuration_field::view] = 1;
    value[configuration_field::music_restart] = 2;
    value[configuration_field::graphic_detail] = 2;
    value[configuration_field::screen_flip] = 1;
    value[configuration_field::first_key] = 'F';
    value[configuration_field::first_key + 1] = 'J';
    value[configuration_field::first_key + 2] = 'G';
    value[configuration_field::first_key + 3] = 'H';
    value[configuration_field::first_key + 4] = ' ';
    value[configuration_field::sound_16bit] = 1;
    value[configuration_field::sound_stereo] = 1;
    value[13] = 0;
    value[configuration_field::sound_channels] = 3;
    value[configuration_field::sound_rate] = 22050;
    value[16] = 1024;
    value[configuration_field::music_volume] = 63;
    value[configuration_field::effects_volume] = 127;
    return value;
}
Bytes serialize_configuration(Configuration value) {
    value[configuration_field::version] = 1;
    Bytes result(configuration_record_bytes);
    for (std::size_t i = 0; i < value.size(); i++)
        put32(result, i * 4, value[i]);
    return result;
}
Configuration parse_configuration(std::span<const std::uint8_t> bytes) {
    require(bytes.size() >= configuration_record_bytes && u32(bytes, 0) == 1, "Unsupported configuration record");
    Configuration value{};
    for (std::size_t i = 0; i < value.size(); i++)
        value[i] = u32(bytes, i * 4);
    return value;
}
// 42 key bytes (excluding the string terminator), repeated in original arrays.
static constexpr char score_key[] = "yX7&*--Mm>?ui%u2fG[p64+=*hpiB ^|j)jK!hou,!";
static_assert(sizeof(score_key) == 43);
static std::uint32_t score_checksum(const Scores& value) {
    std::uint32_t checksum = 0;
    for (std::size_t i = 0; i < value.payload.size(); i++)
        checksum = (checksum + value.payload[i]) ^ std::uint8_t(score_key[(i + 4) % 42]);
    return checksum;
}
Bytes serialize_scores(const Scores& value) {
    Bytes result(Scores::record_bytes);
    put32(result, 0, 1);
    put32(result, 4, score_checksum(value));
    for (std::size_t i = 0; i < value.payload.size(); i++)
        result[i + Scores::header_bytes] = value.payload[i] ^ std::uint8_t(std::uint8_t(score_key[(i + 17) % 42]) + 0x80);
    return result;
}
Scores parse_scores(std::span<const std::uint8_t> bytes) {
    require(bytes.size() >= Scores::record_bytes && u32(bytes, 0) == 1, "Unsupported score record");
    Scores value;
    for (std::size_t i = 0; i < value.payload.size(); i++)
        value.payload[i] = bytes[i + Scores::header_bytes] ^ std::uint8_t(std::uint8_t(score_key[(i + 17) % 42]) + 0x80);
    require(score_checksum(value) == u32(bytes, 4), "Score checksum mismatch");
    return value;
}
std::string variant_name(unsigned value) {
    std::ostringstream out;
    out << std::uppercase << std::hex << value;
    return out.str();
}
DisplayMode display_mode(unsigned variant) {
    require((variant & 15) <= 6 && (variant & 15) != 3, "Invalid view code");
    switch (variant >> 4) {
        case 1:
            return {640, 480, 1};
        case 2:
            return {800, 600, 1};
        case 3:
            return {1024, 768, 1};
        case 5:
            return {640, 480, 2};
        case 6:
            return {800, 600, 2};
        case 7:
            return {1024, 768, 2};
        default:
            throw std::runtime_error("Invalid display code");
    }
}
static std::uint32_t rgb555(std::uint16_t p) {
    const auto scale = [](unsigned c) { return (c * 255 + 15) / 31; };
    return scale((p >> 10) & 31) << 16 | scale((p >> 5) & 31) << 8 | scale(p & 31);
}
int table_depth_top(unsigned variant) {
    const auto mode = display_mode(variant);
    return (variant & 4) ? mode.height / 4 : 0;
}
Bitmap load_table(const std::filesystem::path& root, unsigned variant) {
    return load_bitmap(root / variant_name(variant) / "TABLE/VIEW.BIT", display_mode(variant));
}
std::array<std::uint32_t, 256> decode_palette(std::span<const std::uint8_t> bytes) {
    require(bytes.size() == 768, "Invalid palette length");
    std::array<std::uint32_t, 256> palette{};
    for (std::size_t i = 0; i < palette.size(); ++i)
        for (unsigned c = 0; c < 3; ++c) {
            require(bytes[i * 3 + c] <= 63, "Invalid six-bit palette component");
            palette[i] |= (unsigned(bytes[i * 3 + c]) << 2) << (16 - c * 8);
        }
    return palette;
}
Bitmap load_bitmap(const std::filesystem::path& path, DisplayMode mode) {
    require(mode.width > 0 && mode.height > 0 && (mode.bytes_per_pixel == 1 || mode.bytes_per_pixel == 2), "Invalid raw bitmap dimensions or pixel format");
    Bitmap image;
    image.width = mode.width;
    image.height = mode.height;
    auto bytes = read_file(path);
    const std::size_t count = std::size_t(mode.width) * mode.height;
    require(bytes.size() == count * mode.bytes_per_pixel, "Bitmap dimensions disagree: ", path);
    image.pixels.resize(count);
    if (mode.bytes_per_pixel == 1) {
        auto palette_path = path;
        auto pal = read_file(palette_path.replace_extension(".PAL"));
        image.palette = decode_palette(pal);
        for (std::size_t i = 0; i < count; i++)
            image.pixels[i] = image.palette[bytes[i]];
    } else
        for (std::size_t i = 0; i < count; i++)
            image.pixels[i] = rgb555(u16(bytes, i * 2));
    return image;
}
void write_bmp(const std::filesystem::path& path, const Bitmap& image) {
    Bytes bytes(54 + image.pixels.size() * 4);
    bytes[0] = 'B';
    bytes[1] = 'M';
    put32(bytes, 2, static_cast<std::uint32_t>(bytes.size()));
    put32(bytes, 10, 54);
    put32(bytes, 14, 40);
    put32(bytes, 18, image.width);
    put32(bytes, 22, std::uint32_t(-image.height));
    put16(bytes, 26, 1);
    put16(bytes, 28, 32);
    for (std::size_t i = 0; i < image.pixels.size(); i++)
        put32(bytes, 54 + i * 4, image.pixels[i]);
    write_file(path, bytes);
}
Container Container::load(const std::filesystem::path& path, std::size_t origin) {
    Container bank;
    bank.bytes = read_file(path);
    bank.origin = origin;
    auto first = u32(bank.bytes, origin);
    require(first >= 8 && first % 4 == 0 && origin + first <= bank.bytes.size(), "Invalid offset table: ", path);
    for (std::size_t o = 0; o < first; o += 4) {
        auto next = u32(bank.bytes, origin + o);
        require(next >= first && next <= bank.bytes.size() - origin && (bank.offsets.empty() || next >= bank.offsets.back()), "Invalid container offset");
        bank.offsets.push_back(next);
    }
    require(bank.offsets.back() + origin == bank.bytes.size(), "Container does not end at EOF");
    return bank;
}
std::span<const std::uint8_t> Container::block(std::size_t frame) const {
    require(frame < size(), "Sprite frame out of range");
    return std::span<const std::uint8_t>(bytes).subspan(origin + offsets[frame], offsets[frame + 1] - offsets[frame]);
}
std::vector<Span> decode_sprite(const Container& bank, std::size_t frame, int bpp) {
    require(bpp >= 0 && bpp <= 2, "Invalid sprite color depth");
    auto bytes = bank.block(frame);
    require(bytes.size() >= 6, "Truncated sprite header");
    int x = std::int16_t(u16(bytes, 0)), y = std::int16_t(u16(bytes, 2));
    auto count = u16(bytes, 4);
    std::size_t cursor = 6;
    std::vector<Span> spans;
    if (count == 0) {
        require(bytes.size() == 6, "Unexpected empty sprite framing");
        return spans;
    }
    for (;;) {
        require(count <= 16384 && cursor + std::size_t(count) * bpp < bytes.size(), "Truncated sprite span");
        Span span{x, y, {}};
        span.pixels.reserve(count);
        for (unsigned i = 0; i < count; i++)
            span.pixels.push_back(bpp == 0 ? 0 : bpp == 1 ? bytes[cursor + i] : u16(bytes, cursor + i * 2));
        spans.push_back(std::move(span));
        cursor += std::size_t(count) * bpp;
        x += count;
        unsigned deltaY = bytes[cursor++];
        if (deltaY == 255)
            break;
        x += std::int16_t(u16(bytes, cursor));
        y += deltaY;
        count = u16(bytes, cursor + 2);
        cursor += 4;
    }
    require(cursor == bytes.size(), "Unconsumed sprite bytes");
    return spans;
}
void draw_sprite(Bitmap& image, const Container& bank, std::size_t frame, int bpp, int x, int y) {
    require(bpp == 1 || bpp == 2, "Geometry masks need a separate compositing operation");
    for (const auto& span : decode_sprite(bank, frame, bpp)) {
        int row = span.y + y;
        if (row < 0 || row >= image.height)
            continue;
        for (std::size_t i = 0; i < span.pixels.size(); i++) {
            int col = span.x + x + static_cast<int>(i);
            if (col < 0 || col >= image.width)
                continue;
            image.pixels[std::size_t(row) * image.width + col] = bpp == 1 ? image.palette[span.pixels[i]] : rgb555(span.pixels[i]);
        }
    }
}
TileMap TileMap::load(const std::filesystem::path& path, int columns) {
    require(columns > 0, "Invalid tile map width");
    auto bytes = read_file(path);
    require(bytes.size() % (columns * 2) == 0, "Invalid tile map dimensions");
    TileMap value;
    value.columns = columns;
    value.rows = static_cast<int>(bytes.size() / (columns * 2));
    auto bankPath = path;
    bankPath.replace_extension(".SET");
    value.tiles = read_file(bankPath);
    require(value.tiles.size() % 64 == 0, "Invalid tile bank length");
    for (std::size_t o = 0; o < bytes.size(); o += 2) {
        auto tile = u16(bytes, o);
        require(std::size_t(tile) * 64 + 64 <= value.tiles.size(), "Tile outside bank");
        value.cells.push_back(tile);
    }
    return value;
}
static std::size_t sample_offset(const TileMap& map, int x, int y) {
    require(x >= 0 && y >= 0 && x < map.columns * 8 && y < map.rows * 8, "Sample outside tile map");
    return std::size_t(map.cells[std::size_t(y / 8) * map.columns + x / 8]) * 64 + (y & 7) * 8 + (x & 7);
}
std::uint8_t TileMap::sample(int x, int y) const {
    return tiles.at(sample_offset(*this, x, y));
}
std::uint16_t TileMap::sample_word(int x, int y) const {
    return u16(tiles, sample_offset(*this, x, y));
}
void TileMap::patch(const Container& bank, std::size_t frame) {
    require(bank.origin == 16, "Depth patch needs a ZMP container");
    unsigned x = u32(bank.bytes, 0), y = u32(bank.bytes, 4), w = u32(bank.bytes, 8), h = u32(bank.bytes, 12);
    auto data = bank.block(frame);
    require(x <= unsigned(columns) && y <= unsigned(rows) && w <= unsigned(columns) - x && h <= unsigned(rows) - y && data.size() == std::size_t(w) * h * 2, "Invalid depth patch rectangle");
    for (unsigned row = 0; row < h; row++)
        for (unsigned col = 0; col < w; col++) {
            auto tile = u16(data, (std::size_t(row) * w + col) * 2);
            require(std::size_t(tile) * 64 + 64 <= tiles.size(), "Depth patch tile outside bank");
            cells[std::size_t(y + row) * columns + x + col] = tile;
        }
}
std::size_t validate_assets(const std::filesystem::path& root) {
    std::size_t frames = 0;
    for (unsigned mode : {1, 2, 3, 5, 6, 7})
        for (unsigned view : {0, 1, 2, 4, 5, 6}) {
            unsigned id = mode * 16 + view;
            auto display = display_mode(id);
            auto directory = root / variant_name(id);
            (void)load_table(root, id);
            auto depth = TileMap::load(directory / "TABLE/VIEW.MAP", display.width / 8);
            for (auto name : {"FLIPL", "FLIPR", "FLIPU", "SPINA", "SPINB"}) {
                auto bank = Container::load(directory / "TABLE" / (std::string(name) + ".ZMP"), 16);
                for (std::size_t i = 0; i < bank.size(); i++)
                    depth.patch(bank, i);
            }
            for (auto name : {"BALL", "BALLSHAD", "FLIPL", "FLIPR", "FLIPU", "SPINNERA", "SPINNERB", "LIGHTOFF", "LIGHTHLF", "LIGHTON", "UNFLIPL", "UNFLIPR", "UNFLIPU", "UNSPINL", "UNSPINR"}) {
                auto bank = Container::load(directory / "SPRITES" / (std::string(name) + ".SPR"));
                const std::string kind = name;
                int bpp = (kind == "BALLSHAD" || kind.starts_with("UN")) ? 0 : display.bytes_per_pixel;
                for (std::size_t i = 0; i < bank.size(); i++) {
                    try {
                        (void)decode_sprite(bank, i, bpp);
                        frames++;
                    } catch (const std::exception& error) {
                        throw std::runtime_error(utf8_path(directory) + "/SPRITES/" + name + " frame " + std::to_string(i) + ": " + error.what());
                    }
                }
            }
        }
    for (auto name : {"COLLIDE0", "COLLIDE1", "COLLIDE2", "HEIGHT1", "HEIGHT2", "SWITCH0", "SWITCH1"})
        (void)TileMap::load(root / "COLLIDE" / (std::string(name) + ".MAP"), 40);
    return frames;
}
} // namespace web
