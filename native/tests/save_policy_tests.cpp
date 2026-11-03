#include "../configuration_store.hpp"
#include "../score_store.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
unsigned checks = 0;
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
    ++checks;
}
// Exercises portable policy with a controllable storage boundary. Real locking,
// flush and rename behavior is covered by the Windows persistence tests.
class Storage final : public web::SaveStorage {
public:
    bool fail_lock = false, fail_replace = false;
    unsigned writes = 0;
    void lock(const std::filesystem::path&) override {
        if (fail_lock)
            throw std::runtime_error("Injected lock failure");
    }
    void replace(const std::filesystem::path& target, std::span<const std::uint8_t> bytes) override {
        if (fail_replace)
            throw std::runtime_error("Injected replacement failure");
        web::write_file(target, bytes);
        ++writes;
    }
};
} // namespace
int main() {
    try {
        const auto directory = std::filesystem::temp_directory_path() / ("theweb-save-policy-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("Fixture directory already exists");
        struct Cleanup {
            std::filesystem::path path;
            ~Cleanup() {
                std::error_code ignored;
                std::filesystem::remove_all(path, ignored);
            }
        } cleanup{directory};
        auto storage = std::make_unique<Storage>();
        auto& control = *storage;
        web::ConfigurationStore store(directory, std::move(storage));
        check(!store.load() && store.error().empty(), "Missing configuration did not retain defaults");
        web::GameOptions options;
        check(store.save(options), "Initial save failed");
        const auto original = web::read_file(directory / "wgame.cfg");
        check(store.save(options) && control.writes == 1, "Unchanged save called the backend");
        options.volume(10);
        control.fail_replace = true;
        check(!store.save(options) && !store.error().empty(), "Replacement failure was hidden");
        check(web::read_file(directory / "wgame.cfg") == original, "Replacement failure changed the previous file");
        control.fail_replace = false;
        check(store.save(options) && store.error().empty(), "Retry after backend failure lost the previous snapshot");
        web::write_file(directory / "wgame.cfg", original);
        check(!store.save(options) && control.writes == 2, "External edit was overwritten");
        auto blocked = std::make_unique<Storage>();
        blocked->fail_lock = true;
        web::ScoreStore scores(directory, std::move(blocked));
        check(!scores.load() && !scores.error().empty(), "Lock failure was hidden");
        check(!scores.save({}), "Unlocked store wrote a file");
        web::write_file(directory / "bad.dat", web::Bytes{1});
        web::SaveFile damaged(directory, "bad.dat", 2, [](auto) {}, std::make_unique<Storage>());
        check(!damaged.load() && !damaged.error().empty(), "Invalid size was accepted");
        check(!damaged.save({1, 2}) && web::read_file(directory / "bad.dat") == web::Bytes{1}, "Damaged file was overwritten");
        bool duplicate_load = false;
        try {
            damaged.load();
        } catch (const std::logic_error&) {
            duplicate_load = true;
        }
        check(duplicate_load, "Loaded store acquired a second lock");
        std::cout << checks << " portable save policy checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
