#include "../platform/linux/save_storage.hpp"
#include "../configuration_store.hpp"
#include "../host_settings.hpp"
#include "../score_store.hpp"
#include <fstream>
#include <iostream>
#include <unistd.h>

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("theweb-storage-" + std::to_string(::getpid()) + "-\xc3\xa4");
    unsigned checks = 0;
    const auto check = [&](bool value) {
        if (!value)
            throw std::runtime_error("Linux storage check " + std::to_string(checks + 1));
        ++checks;
    };
    try {
        std::filesystem::create_directories(root);
        {
            web::ConfigurationStore first(root, web::make_posix_save_storage());
            check(!first.load());
            web::GameOptions options;
            options.volume(79);
            check(first.save(options));
            web::ConfigurationStore second(root, web::make_posix_save_storage());
            check(!second.load());
            check(!second.error().empty());
            check(!second.save(options));
        }
        {
            web::ConfigurationStore reopened(root, web::make_posix_save_storage());
            const auto loaded = reopened.load();
            check(loaded && loaded->volume() == 79);
            auto edited = *loaded;
            edited.volume(31);
            check(reopened.save(edited));
        }
        {
            web::ConfigurationStore first(root, web::make_posix_save_storage());
            check(first.load()->volume() == 31);
            std::ofstream(root / "wgame.cfg", std::ios::binary | std::ios::trunc) << "broken";
            check(!first.save(web::GameOptions{}));
            check(web::read_file(root / "wgame.cfg") == web::Bytes({'b', 'r', 'o', 'k', 'e', 'n'}));
        }
        {
            web::ConfigurationStore damaged(root, web::make_posix_save_storage());
            check(!damaged.load());
            check(!damaged.error().empty());
            check(!damaged.save(web::GameOptions{}));
            web::HostSettingsStore host(root, web::make_posix_save_storage());
            check(!host.load());
            web::HostSettings settings;
            settings.fullscreen = false;
            check(host.save(settings));
        }
        {
            web::HostSettingsStore host(root, web::make_posix_save_storage());
            check(!host.load()->fullscreen);
        }
        // Failed rename preserves the preexisting target and removes only the
        // temporary file created by this operation.
        {
            auto storage = web::make_posix_save_storage();
            storage->lock(root / "test.lock");
            std::filesystem::create_directory(root / "directory-target");
            bool failed = false;
            try {
                storage->replace(root / "directory-target", web::Bytes{1, 2, 3});
            } catch (const std::exception&) {
                failed = true;
            }
            check(failed && std::filesystem::is_directory(root / "directory-target"));
            for (const auto& entry : std::filesystem::directory_iterator(root))
                check(entry.path().filename().string().find(".tmp-") == std::string::npos);
        }
        std::filesystem::remove_all(root);
        std::cout << "PASS " << checks << " Linux storage checks\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(root);
        return 1;
    }
}
