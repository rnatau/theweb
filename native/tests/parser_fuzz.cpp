#include "../host_settings.hpp"
#include "../input_timeline.hpp"
#include "../sound.hpp"
#include <algorithm>
#include <array>
#include <iostream>

namespace {
template <class F> void parse(F operation) {
    try {
        operation();
    } catch (const std::exception&) {
        // Malformed records are expected to be rejected by the codecs.
    }
}
void exercise(std::span<const std::uint8_t> bytes) {
    parse([&] { web::GameOptions{web::parse_configuration(bytes)}; });
    parse([&] { web::parse_scores(bytes); });
    parse([&] { web::HostSettings::parse(bytes); });
    parse([&] { web::parse_timed_input(web::Bytes(bytes.begin(), bytes.end())); });
    parse([&] { web::decode_palette(bytes); });
    parse([&] {
        web::Container container{
            web::Bytes(bytes.begin(), bytes.end()),
            {0, std::uint32_t(bytes.size())},
            0
        };
        web::decode_sprite(container, 0, 2);
    });
    parse([&] {
        std::array<std::int16_t, 64> pcm{};
        const auto blocks = bytes.first(std::min(bytes.size() / 4, pcm.size() / 8) * 4);
        web::SoundDecoder::decode(blocks, 0, std::span(pcm).first(blocks.size() * 2));
    });
}
} // namespace
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    if (size <= 4096)
        exercise({data, size});
    return 0;
}
#ifndef WEB_LIBFUZZER
int main() {
    // Reproducible malformed/truncated/mutated inputs; no original assets.
    std::array<web::Bytes, 4> seeds{
        web::serialize_configuration(web::default_configuration()),
        web::serialize_scores({}),
        web::HostSettings{}.serialize(),
        web::serialize_timed_input({2, {{0, web::InputSource::keyboard, 1}}})};
    std::size_t cases = 0;
    for (const auto& seed : seeds) {
        for (std::size_t length = 0; length <= seed.size(); ++length) {
            exercise(std::span(seed).first(length));
            ++cases;
        }
        for (std::size_t offset = 0; offset < seed.size(); ++offset)
            for (const auto mask : {1, 0x80, 0xff}) {
                auto mutation = seed;
                mutation[offset] ^= std::uint8_t(mask);
                exercise(mutation);
                ++cases;
            }
    }
    std::cout << "PASS: " << cases << " parser mutation/truncation cases\n";
}
#endif
