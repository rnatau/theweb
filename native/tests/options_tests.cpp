#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../configuration_store.hpp"
#include "../platform/win32/save_storage.hpp"
#include "../options.hpp"
#include "../sound.hpp"
#include <iostream>

namespace {
unsigned checks = 0;
void check(bool valid, const char* message) {
    if (!valid)
        throw std::runtime_error(message);
    ++checks;
}
template <class F> void rejects(F function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    check(rejected, "Invalid option was accepted");
}
} // namespace
int main(int argc, char** argv) {
    try {
        if (argc == 4 && std::string_view(argv[1]) == "--audio-config") {
            const auto input = web::read_file(argv[2]);
            web::Bytes output;
            for (std::size_t offset = 0; offset < input.size(); offset += 84) {
                const web::GameOptions options(web::parse_configuration(std::span(input).subspan(offset, 84)));
                for (auto value : {options.sound_rate(), options.sound_channels(), unsigned(options.sound_stereo()), unsigned(options.sound_16bit())})
                    for (unsigned byte = 0; byte < 4; ++byte)
                        output.push_back(std::uint8_t(value >> (byte * 8)));
            }
            web::write_file(argv[3], output);
            return 0;
        }
        if (argc == 4 && std::string_view(argv[1]) == "--oracle") {
            const auto input = web::read_file(argv[2]);
            if (input.size() % 84)
                throw std::runtime_error("Configuration fixtures need 84-byte records");
            web::Bytes output;
            const auto append = [&](std::uint32_t value) {
                for (unsigned i = 0; i < 4; ++i)
                    output.push_back(std::uint8_t(value >> (8 * i)));
            };
            for (std::size_t offset = 0; offset < input.size(); offset += 84) {
                const web::GameOptions options(web::parse_configuration(std::span(input).subspan(offset, 84)));
                append(options.view());
                for (unsigned i = 0; i < 5; ++i)
                    append(options.key(i));
                append(options.sound_enabled());
                append(options.volume());
                append(options.music_restart_policy());
                append(options.display_variant());
                append(options.graphic_detail() == 1);
                append(options.graphic_detail() == 3);
                append(options.music_volume());
            }
            web::write_file(argv[3], output);
            return 0;
        }
        web::GameOptions options;
        check(!options.visual_nudge(), "Faithful settings must retain original visual nudge Off default");
        auto disabled_nudge = options.configuration();
        disabled_nudge[web::configuration_field::visual_nudge] = 0;
        check(!web::GameOptions(disabled_nudge).visual_nudge(), "Saved visual nudge preference was ignored");
        for (unsigned policy = 1; policy <= 3; ++policy) {
            auto value = options.configuration();
            value[3] = policy;
            check(web::GameOptions(value).music_restart_policy() == policy - 1, "Original music restart mapping differs");
        }
        for (unsigned policy : {0u, 4u}) {
            auto value = options.configuration();
            value[3] = policy;
            rejects([&] { web::GameOptions invalid(value); });
        }
        check(options.view() == 0 && options.screen_mode() == 6 && options.display_variant() == 0x70, "Native maximum-quality default view changed");
        for (unsigned mode : {0u, 7u}) {
            auto value = options.configuration();
            value[1] = mode;
            rejects([&] { web::GameOptions invalid(value); });
        }
        for (unsigned detail : {0u, 4u}) {
            auto value = options.configuration();
            value[4] = detail;
            rejects([&] { web::GameOptions invalid(value); });
        }
        for (unsigned i = 0; const auto view : {2u, 1u, 0u, 6u, 5u, 4u}) {
            options.view(view);
            check(options.configuration()[2] == ++i && options.view() == view, "Original view mapping differs");
        }
        auto preserved = options.configuration();
        preserved[1] = 6;
        preserved[13] = 0xfedcba98;
        preserved[19] = 0x76543210;
        preserved[20] = 0x12345678;
        options = web::GameOptions(preserved);
        options.key(0, 'A');
        options.volume(35);
        options.music_volume(91);
        check(options.configuration()[17] == 91 && options.music_enabled(), "Music volume did not use original DWORD17");
        options.music_volume(0);
        check(!options.music_enabled() && options.sound_enabled(), "Music off disabled sound effects");
        rejects([&] { options.music_volume(256); });
        options.view(2);
        check(options.configuration()[1] == 6, "Changing camera damaged the original screen-mode field");
        check(options.configuration()[13] == preserved[13] && options.configuration()[19] == preserved[19] && options.configuration()[20] == preserved[20], "Editing options damaged unowned words");
        std::array<bool, 256> held{};
        for (unsigned i = 0; const auto bit : {1u, 2u, 0x200u, 0x100u, 0x20u}) {
            held.fill(false);
            held[options.key(i++)] = true;
            check(options.actions(held) == bit, "Rebound game control maps to wrong action");
        }
        held.fill(false);
        held['A'] = held[37] = true;
        held['A'] = false;
        check(options.actions(held) == 0, "Original arrow volume key drove a flipper");
        held['S'] = held[13] = held['C'] = true;
        check(options.actions(held) == (4 | 8 | 16), "Fixed commands lost under custom controls");
        for (auto key : {'S', 'P', 'C', '\r', '%', '&', '\'', '(', '\x1b'})
            rejects([&] { options.key(0, unsigned(key)); });
        rejects([&] { options.key(0, options.key(1)); });
        rejects([&] { options.key(5, 'A'); });
        rejects([&] { options.volume(128); });
        rejects([&] { options.view(3); });
        options.sound_enabled(false);
        check(!options.sound_enabled() && options.configuration()[15] == 0, "Disabling effects changed the wrong field");
        options.sound_enabled(true);
        check(options.sound_enabled() && options.configuration()[15] == 22050, "Enabling effects did not restore a valid rate");
        const auto directory = std::filesystem::absolute("build") / ("options-test-" + std::to_string(GetCurrentProcessId()));
        if (std::filesystem::exists(directory))
            throw std::runtime_error("Fixture folder already exists");
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        } cleanup{directory};
        const auto path = directory / "wgame.cfg";
        {
            web::ConfigurationStore store(directory, web::make_win32_save_storage());
            check(!store.load() && store.error().empty(), "Missing config did not retain defaults");
            check(store.save(options), "Options save failed");
            check(web::read_file(path).size() == 84 && web::parse_configuration(web::read_file(path))[0] == 1, "Original config size/version differs");
            const auto timestamp = std::filesystem::last_write_time(path);
            check(store.save(options) && std::filesystem::last_write_time(path) == timestamp, "Unchanged configuration was rewritten");
            web::ConfigurationStore competing(directory, web::make_win32_save_storage());
            check(!competing.load() && !competing.save(options) && !competing.error().empty(), "Competing options writer was accepted");
        }
        {
            web::ConfigurationStore store(directory, web::make_win32_save_storage());
            const auto loaded = store.load();
            check(
                loaded && loaded->key(0) == 'A' && loaded->volume() == 35 && loaded->view() == 2 && loaded->configuration()[13] == preserved[13],
                "Options or reserved values did not survive reopening");
        }
        for (unsigned kind = 0; kind < 4; ++kind) {
            auto invalid = options.configuration();
            if (kind == 1)
                invalid[2] = 99;
            if (kind == 2)
                invalid[6] = 'S';
            if (kind == 3)
                invalid[18] = 128;
            auto bytes = web::serialize_configuration(invalid);
            if (kind == 0)
                bytes.resize(13);
            web::write_file(path, bytes);
            web::ConfigurationStore store(directory, web::make_win32_save_storage());
            check(!store.load() && !store.error().empty() && !store.save(options) && web::read_file(path) == bytes, "Invalid config was overwritten or silently accepted");
        }
        web::SoundBank bank;
        bank.samples = {
            {12700, -12700}
        };
        bank.definitions[1] = {1, 1, {0}};
        web::SoundMixer mixer(bank);
        std::array<std::int16_t, 4> output;
        mixer.volume(63);
        mixer.sound({1});
        mixer.render(output);
        check(output == std::array<std::int16_t, 4>{6307, 6307, -6307, -6307}, "Original -608-hundredths-dB master volume does not scale signed stereo PCM");
        mixer.volume(0);
        mixer.sound({1});
        mixer.render(output);
        check(output == std::array<std::int16_t, 4>{}, "Zero volume is not silent");
        rejects([&] { mixer.volume(128); });
        std::cout << checks << " options, persistence and mixer checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
