#pragma once
#include "web.hpp"
#include <array>
#include <vector>

namespace web {
struct SoundRequest {
    std::int32_t id = 0, pan = 0, attenuation = 0, group = -1;
};
using SoundRequestFilter = void (*)(SoundRequest&);
class SoundSink {
public:
    virtual ~SoundSink() = default;
    virtual void sound(SoundRequest) = 0;
    virtual void stop(int = -1, int = -1, int = -1) {}
    virtual bool active(int = -1, int = -1, int = -1) const {
        return false;
    }
};
// Original 0x418c0c, with table initializer 0x41b6c0. State includes the
// unsigned low predictor, carry bits and the step index in the high byte.
class SoundDecoder {
public:
    static constexpr unsigned step_count = 61, lookup_stride = 256;
    static constexpr unsigned block_bytes = 4, samples_per_block = 8;
    static std::uint32_t decode(std::span<const std::uint8_t> blocks, std::uint32_t state, std::span<std::int16_t> pcm);
    static std::array<std::uint32_t, step_count * lookup_stride> table();
};
struct SoundDefinition {
    enum Flag : std::uint32_t {
        allow_overlap = 1,
        original_o = 2, // Retained source flags with no recovered mixer effect.
        restart_duplicate = 4,
        replace_equal_priority = 8,
        original_l = 16,
        category_u = 32,
        disabled = 64,
        original_i = 256
    };
    std::uint32_t priority = 0, flags = 0;
    std::vector<std::int32_t> sequence;
};
class SoundBank {
public:
    static constexpr unsigned shipped_sample_count = 183, definition_count = 192;
    static SoundBank load(const std::filesystem::path& assets, unsigned rate = 44100);
    unsigned rate = 44100;
    std::uint16_t extra_sample_step = 0; // Sample183 header; original -128 pan alias.
    std::vector<std::vector<std::int16_t>> samples;
    std::array<SoundDefinition, definition_count + 1> definitions;
};
struct SoundVoice {
    std::uint32_t status = 0, id = 0, priority = 0;
    std::int32_t pan = 0, attenuation = 0, group = -1;
    std::size_t element = 0, position = 0;
    bool rendered_complete = false; // PCM generated; queued device playback may still be pending.
};
// Priority/duplicate/group selection follows 0x4177a0. Device timing is outside
// this owner; decoded samples and an independent mixer replace DirectSound.
class SoundMixer : public SoundSink {
public:
    explicit SoundMixer(const SoundBank& bank, unsigned voices = 3, SoundRequestFilter filter = nullptr);
    int request(SoundRequest);
    void sound(SoundRequest request) override {
        this->request(request);
    }
    bool active(int id = -1, int channel = -1, int group = -1) const override;
    void stop(int id = -1, int channel = -1, int group = -1) override;
    void render(std::span<std::int16_t> stereo);
    void volume(unsigned value);
    void complete(unsigned channel); // Device completion releases retained arbitration/query state.
    const std::vector<SoundVoice>& voices() const {
        return voices_;
    }
    std::vector<SoundVoice>& voices() {
        return voices_;
    }
    // Original gates at 0x42ca94/98/9c; the U category's product meaning is open.
    bool enabled = true, category_u_enabled = true, other_categories_enabled = true;
    bool retain_until_played = false; // Production device supplies actual buffer completions.

private:
    const SoundBank& bank_;
    std::vector<SoundVoice> voices_;
    unsigned volume_ = 127;
    SoundRequestFilter filter_;
};
} // namespace web
