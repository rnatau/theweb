#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../platform/win32/save_storage.hpp"
#include "../score_store.hpp"
#include "../rules/score_records.hpp"
#if WEB_ASSET_TESTS
#include "../gameplay.hpp"
#endif
#include <iostream>

namespace {
unsigned checks = 0;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
    ++checks;
}
} // namespace
int main() {
    try {
        const auto directory = std::filesystem::absolute("build") / ("score-store-test-" + std::to_string(GetCurrentProcessId()));
        if (std::filesystem::exists(directory))
            throw std::runtime_error("Fixture folder already exists");
        std::filesystem::create_directories(directory);
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        } cleanup{directory};
        web::rules::ScoreRecords records;
        web::rules::ReplayState replay;
        records.ordinary()[0] = {
            {'B', 'E', 'R', 0},
            {12345678, 2}
        };
        records.loops().reserved = 0xfedcba98;
        records.combos().reserved = 0x76543210;
        replay.credits = 7;
        auto saved = records.save(replay);
        const auto file = directory / "game.dat";
        {
            web::ScoreStore store(directory, web::make_win32_save_storage());
            check(!store.load() && store.error().empty(), "Missing save should use defaults");
            check(store.save(saved), "Initial save failed");
            check(web::parse_scores(web::read_file(file)).payload == saved.payload, "Saved bytes differ");
            const auto timestamp = std::filesystem::last_write_time(file);
            check(store.save(saved) && std::filesystem::last_write_time(file) == timestamp, "Unchanged scores rewrote the file");
            web::ScoreStore competing(directory, web::make_win32_save_storage());
            check(!competing.load() && !competing.error().empty(), "Second instance acquired score lock");
            check(!competing.save(saved), "Second instance overwrote scores");
        }
        {
            web::ScoreStore store(directory, web::make_win32_save_storage());
            auto loaded = store.load();
            check(loaded && loaded->payload == saved.payload, "Reopening lost data or reserved words");
#if WEB_ASSET_TESTS
            web::PinballGame game("data/PC_DAT", web::PinballGame::InputTiming::post_poll_fixture);
            game.load_scores(*loaded);
            check(game.save_scores().payload == saved.payload, "Game import/export differs");
            game.start();
            check(game.save_scores().payload == saved.payload, "Starting a game reset saved records");
            bool rejected = false;
            try {
                game.load_scores(saved);
            } catch (const std::logic_error&) {
                rejected = true;
            }
            check(rejected, "Live game allowed late score import");
#endif
            auto changed = saved;
            changed.payload[8] = 8;
            const auto before = web::read_file(file);
            auto held = CreateFileW(file.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
            check(held != INVALID_HANDLE_VALUE, "Cannot hold save fixture");
            check(!store.save(changed) && !store.error().empty(), "Blocked replacement should fail");
            CloseHandle(held);
            check(web::read_file(file) == before, "Failed replacement damaged previous save");
            check(store.save(changed) && store.error().empty(), "Retry after sharing violation failed");
            saved = changed;
        }
        {
            web::ScoreStore store(directory, web::make_win32_save_storage());
            check(bool(store.load()), "Cannot reload replaced file");
            const web::Bytes outside{1, 2, 3};
            web::write_file(file, outside);
            check(!store.save(saved) && web::read_file(file) == outside, "External replacement was overwritten");
        }
        for (unsigned kind = 0; kind < 4; ++kind) {
            auto damaged = web::serialize_scores(saved);
            if (kind == 0)
                damaged.resize(13);
            if (kind == 1)
                damaged[25] ^= 64;
            if (kind >= 2) {
                auto unsafe = saved;
                if (kind == 2)
                    unsafe.payload[12] = 1; // Valid checksum, invalid glyph.
                else
                    unsafe.payload[8] = 255; // Valid checksum, unsupported credits.
                damaged = web::serialize_scores(unsafe);
            }
            web::write_file(file, damaged);
            web::ScoreStore store(directory, web::make_win32_save_storage());
            check(!store.load() && !store.error().empty(), "Invalid save accepted");
            check(!store.save(saved) && web::read_file(file) == damaged, "Invalid save was overwritten");
        }
        const auto unicode = directory / L"scores-\u00e4\u65e5";
        {
            web::ScoreStore store(unicode, web::make_win32_save_storage());
            check(!store.load() && store.save(saved), "Unicode save folder failed");
            check(web::parse_scores(web::read_file(unicode / "game.dat")).payload == saved.payload, "Unicode save differed");
            const auto copy = unicode / L"copy-\u65e5.dat";
            const auto bytes = web::serialize_scores(saved);
            web::write_file(copy, bytes);
            check(web::read_file(copy) == bytes, "Unicode file helper round trip failed");
            for (const bool write : {false, true}) {
                const auto missing = unicode / "missing" / L"file-\u65e5.dat";
                bool rejected = false;
                try {
                    if (write)
                        web::write_file(missing, bytes);
                    else
                        (void)web::read_file(missing);
                } catch (const std::runtime_error& error) {
                    const auto encoded = missing.u8string();
                    const auto expected = std::string(write ? "Cannot write " : "Cannot open ") + std::string(encoded.begin(), encoded.end());
                    check(error.what() == expected, "Unicode file failure lost its operation or UTF-8 path");
                    rejected = true;
                }
                check(rejected, "Missing Unicode file did not report an I/O failure");
            }
        }
        {
            web::ScoreStore store(unicode, web::make_win32_save_storage());
            const auto loaded = store.load();
            check(loaded && loaded->payload == saved.payload && store.error().empty(), "Unicode saved scores failed to reopen");
            check(store.save(saved) && store.error().empty(), "Unicode existing-save comparison failed");
        }
        for (const auto& entry : std::filesystem::directory_iterator(directory))
            check(entry.path().filename().native().find(L".tmp-") == std::wstring::npos, "Temporary score file leaked");
        std::cout << checks << " score persistence checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
