#include "sound.hpp"
#include "sound_controls.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <sstream>

namespace web {
namespace {
constexpr std::array<std::int16_t, SoundDecoder::step_count> steps = {1,    1,    1,    1,    2,    2,     3,     3,     4,     5,     6,     7,     8,    10,   12,   14,
                                                                      16,   20,   24,   28,   32,   40,    48,    56,    64,    80,    96,    112,   128,  160,  192,  224,
                                                                      256,  320,  384,  448,  512,  640,   768,   896,   1024,  1280,  1536,  1792,  2048, 2560, 3072, 3584,
                                                                      4096, 5120, 6144, 7168, 8192, 10240, 12288, 14336, 16384, 20480, 24576, 28672, 0};
constexpr int adjustments[] = {-1, -1, -1, -1, 1, 2, 3, 4};
std::vector<std::string> read_definition_lines(const std::filesystem::path& path) {
    const auto bytes = read_file(path);
    std::istringstream stream(std::string(bytes.begin(), bytes.end()));
    std::vector<std::string> result;
    for (std::string line; std::getline(stream, line);) {
        line = line.substr(0, line.find(';'));
        if (line.find_first_not_of(" \t\r") != std::string::npos)
            result.push_back(line);
    }
    return result;
}
std::vector<std::int16_t> decode_sample(std::span<const std::uint8_t> payload) {
    if (payload.size() < 4 || payload.size() % 4)
        throw std::runtime_error("Invalid original sound sample length");
    const auto state = std::uint32_t(u16(payload, 0)) | (std::uint32_t(u16(payload, 2)) << 24);
    if ((state >> 24) > 60)
        throw std::runtime_error("Invalid original sound sample step");
    std::vector<std::int16_t> pcm((payload.size() - 4) * 2);
    SoundDecoder::decode(payload.subspan(4), state, pcm);
    return pcm;
}
bool matches(const SoundVoice& voice, unsigned index, int id, int channel, int group) {
    return voice.status && (id == -1 || voice.id == unsigned(id)) && (channel == -1 || index == unsigned(channel)) && (group == -1 || voice.group == group);
}
} // namespace
std::array<std::uint32_t, SoundDecoder::step_count * SoundDecoder::lookup_stride> SoundDecoder::table() {
    std::array<std::uint32_t, SoundDecoder::step_count * SoundDecoder::lookup_stride> result{};
    for (int quantized = -7; quantized <= 7; ++quantized)
        for (int step = 0; step < int(step_count); ++step) {
            const auto next = std::clamp(step + adjustments[std::abs(quantized)], 0, int(step_count) - 1);
            const auto value = (std::uint32_t(next) << 24) + std::uint16_t(quantized * steps[unsigned(step)]);
            for (unsigned upper = 0; upper < lookup_stride; upper += 16)
                result[unsigned(step) * lookup_stride + upper + (unsigned(quantized) & 15)] = value;
        }
    return result;
}
std::uint32_t SoundDecoder::decode(std::span<const std::uint8_t> blocks, std::uint32_t state, std::span<std::int16_t> pcm) {
    if (blocks.size() % 4 || pcm.size() != blocks.size() * 2)
        throw std::runtime_error("Sound decoder needs complete four-byte blocks");
    static const auto lookup = table();
    std::size_t output = 0;
    for (std::size_t i = 0; i < blocks.size(); i += block_bytes) {
        auto packed = u32(blocks, i);
        for (unsigned nibble = 0; nibble < samples_per_block; ++nibble) {
            const auto index = ((state >> 24) << 8) | (packed & 255);
            if (index >= lookup.size())
                throw std::runtime_error("Sound decoder step outside original table");
            state = (state & 65535) + lookup[index];
            pcm[output++] = std::bit_cast<std::int16_t>(std::uint16_t(state));
            packed >>= 4;
        }
    }
    return state;
}
SoundBank SoundBank::load(const std::filesystem::path& assets, unsigned rate) {
    const auto suffix = rate == 44100 ? "44A" : rate == 22050 ? "22A" : rate == 11025 ? "11A" : rate == 6000 ? "6A" : "";
    if (!*suffix)
        throw std::runtime_error("Unknown original sound sample rate");
    SoundBank bank;
    bank.rate = rate;
    const auto bytes = read_file(assets / "SFX" / (std::string("SFX.") + suffix));
    for (std::size_t offset = 0; offset < bytes.size();) {
        if (offset + 4 > bytes.size())
            throw std::runtime_error("Truncated original sound bank");
        const auto length = u32(bytes, offset);
        offset += 4;
        if (length > bytes.size() - offset)
            throw std::runtime_error("Truncated original sound sample");
        bank.samples.push_back(decode_sample(std::span(bytes).subspan(offset, length)));
        offset += length;
    }
    if (bank.samples.size() != shipped_sample_count)
        throw std::runtime_error("Original sound bank must contain 183 samples");
    auto definitions = read_definition_lines(assets / "SFX/SFXDEFS.DAT");
    std::size_t cursor = 0;
    const auto number = [&] {
        if (cursor >= definitions.size())
            throw std::runtime_error("Truncated sound definitions");
        return std::stoi(definitions[cursor++]);
    };
    if (number() != int(shipped_sample_count))
        throw std::runtime_error("Invalid sound definition sample count");
    cursor += shipped_sample_count; // Build-time source filenames; samples are in the shipped bank.
    if (number() != int(definition_count))
        throw std::runtime_error("Invalid sound definition count");
    for (unsigned id = 1; id <= definition_count; ++id) {
        if (number() != int(id))
            throw std::runtime_error("Unexpected sound definition ID");
        const auto count = number();
        if (count < 1 || count > 1024)
            throw std::runtime_error("Invalid sound sequence length");
        for (int i = 0; i < count; ++i) {
            auto value = number();
            if (value >= int(shipped_sample_count))
                throw std::runtime_error("Invalid sound sequence sample");
            if (value < 0)
                value = std::min(-1, value / int(44100 / rate) / 8);
            bank.definitions[id].sequence.push_back(value);
        }
    }
    if (cursor != definitions.size())
        throw std::runtime_error("Trailing sound definitions");
    for (const auto& line : read_definition_lines(assets / "SFX/SFXFLAGS.DAT")) {
        std::istringstream input(line);
        unsigned id, priority;
        std::string flags;
        if (!(input >> id >> priority >> flags) || id > definition_count)
            throw std::runtime_error("Invalid sound flags");
        auto& definition = bank.definitions[id];
        definition.priority = priority;
        for (auto flag : flags)
            switch (flag) {
                case 'M':
                    definition.flags |= SoundDefinition::allow_overlap;
                    break;
                case 'O':
                    definition.flags |= SoundDefinition::original_o;
                    break;
                case 'R':
                    definition.flags |= SoundDefinition::restart_duplicate;
                    break;
                case 'P':
                    definition.flags |= SoundDefinition::replace_equal_priority;
                    break;
                case 'L':
                    definition.flags |= SoundDefinition::original_l;
                    break;
                case 'U':
                    definition.flags |= SoundDefinition::category_u;
                    break;
                case 'D':
                    definition.flags |= SoundDefinition::disabled;
                    break;
                case 'I':
                    definition.flags |= SoundDefinition::original_i;
                    break;
                default:
                    break; // Original ignores unrecognized flag letters.
            }
    }
    for (unsigned id = 1; id <= definition_count; ++id)
        if (!bank.definitions[id].priority)
            throw std::runtime_error("Missing original sound priority");
    const auto extra = read_file(assets / "SFX" / (std::string("A2620001.") + suffix));
    bank.samples.push_back(decode_sample(extra));
    bank.extra_sample_step = u16(extra, 2);
    bank.definitions[62].sequence.insert(bank.definitions[62].sequence.begin(), int(shipped_sample_count));
    return bank;
}
SoundMixer::SoundMixer(const SoundBank& bank, unsigned voices, SoundRequestFilter filter) :
    bank_(bank),
    voices_(voices),
    filter_(filter) {
    if (!voices || voices > 64)
        throw std::runtime_error("Unsupported sound channel count");
}
int SoundMixer::request(SoundRequest request) {
    if (filter_)
        filter_(request);
    if (!enabled || request.id < 0 || request.id > int(SoundBank::definition_count))
        return -1;
    const auto& definition = bank_.definitions[unsigned(request.id)];
    if ((definition.flags & SoundDefinition::disabled) || (!category_u_enabled && (definition.flags & SoundDefinition::category_u)) ||
        (!other_categories_enabled && !(definition.flags & SoundDefinition::category_u)))
        return -1;
    int selected = -1, duplicate = -1, group = -1;
    std::uint32_t lowest = 0xffffffffu;
    // Original arbitration remembers the last matching duplicate/group, even
    // when group parameters survive a stopped voice. Keep this scan order.
    for (unsigned i = 0; i < voices_.size(); ++i) {
        const auto& voice = voices_[i];
        if (voice.priority < lowest) {
            selected = int(i);
            lowest = voice.priority;
        }
        if (voice.id == unsigned(request.id))
            duplicate = int(i);
        if (voice.group == request.group)
            group = int(i);
    }
    if (request.group != -1 && group != -1) {
        selected = group;
        if (definition.priority < voices_[unsigned(group)].priority)
            return -1;
    } else if (duplicate == -1 || (definition.flags & SoundDefinition::allow_overlap)) {
        if (definition.priority < lowest || (definition.priority == lowest && !(definition.flags & SoundDefinition::replace_equal_priority)))
            return -1;
    } else {
        selected = duplicate;
        if (!(definition.flags & SoundDefinition::restart_duplicate))
            return -1;
    }
    if (selected < 0)
        return -1;
    auto& voice = voices_[unsigned(selected)];
    voice = {};
    voice.status = 1;
    voice.id = unsigned(request.id);
    voice.priority = definition.priority;
    voice.pan = request.pan;
    voice.attenuation = request.attenuation;
    voice.group = request.group;
    return selected;
}
bool SoundMixer::active(int id, int channel, int group) const {
    for (unsigned i = 0; i < voices_.size(); ++i)
        if (matches(voices_[i], i, id, channel, group))
            return true;
    return false;
}
void SoundMixer::stop(int id, int channel, int group) {
    for (unsigned i = 0; i < voices_.size(); ++i)
        if (matches(voices_[i], i, id, channel, group)) {
            // Original stop clears active/priority/id but retains group and parameters.
            voices_[i].status = voices_[i].priority = voices_[i].id = 0;
            voices_[i].rendered_complete = false;
        }
}
void SoundMixer::complete(unsigned channel) {
    auto& voice = voices_.at(channel);
    if (voice.rendered_complete) {
        voice.status = voice.priority = voice.id = 0;
        voice.rendered_complete = false;
    }
}
void SoundMixer::volume(unsigned value) {
    if (value > 127)
        throw std::out_of_range("Effects volume must be between 0 and 127");
    volume_ = value;
}
void SoundMixer::render(std::span<std::int16_t> stereo) {
    if (stereo.size() % 2)
        throw std::runtime_error("Sound mixer needs complete stereo frames");
    std::vector<double> mixed(stereo.size());
    for (auto& voice : voices_) {
        if (!voice.status || voice.rendered_complete)
            continue;
        const auto& sequence = bank_.definitions[voice.id].sequence;
        const auto pan_db = SoundControls::pan_decibels(voice.pan, bank_.extra_sample_step);
        const auto pan_gain = SoundControls::gain(-std::abs(pan_db));
        const auto attenuation = std::min(127, std::abs(std::clamp(voice.attenuation, -128, 127)));
        const auto gain = SoundControls::gain(SoundControls::attenuation_decibels(unsigned(attenuation)));
        const auto left = gain * (pan_db > 0 ? pan_gain : 1.0), right = gain * (pan_db < 0 ? pan_gain : 1.0);
        // Negative sequence entries cause an update boundary in 0x41740c.
        // They are not sample indices. A separator occupies this mixer block.
        for (std::size_t frame = 0; frame < stereo.size() / 2; ++frame) {
            while (voice.element < sequence.size() && sequence[voice.element] >= 0 && voice.position >= bank_.samples.at(unsigned(sequence[voice.element])).size()) {
                ++voice.element;
                voice.position = 0;
            }
            if (voice.element == sequence.size()) {
                if (retain_until_played)
                    voice.rendered_complete = true;
                else
                    voice.status = voice.id = voice.priority = 0;
                break;
            }
            if (sequence[voice.element] < 0) {
                ++voice.element;
                voice.position = 0;
                break;
            }
            const auto value = bank_.samples.at(unsigned(sequence[voice.element]))[voice.position++];
            mixed[frame * 2] += value * left;
            mixed[frame * 2 + 1] += value * right;
            voice.status = 2;
        }
    }
    const auto master = SoundControls::gain(SoundControls::master_decibels(volume_));
    for (std::size_t i = 0; i < stereo.size(); ++i)
        stereo[i] = std::int16_t(std::clamp(std::lround(mixed[i] * master), -32768l, 32767l));
}
} // namespace web
