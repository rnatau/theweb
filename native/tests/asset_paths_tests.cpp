#include "../platform/linux/asset_paths.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("theweb-asset-paths-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto executable = root / "Applications";
    const auto shared = root / "Library/Application Support/theweb/data";
    const auto check = [](bool value) {
        if (!value)
            throw std::runtime_error("Asset path check failed");
    };
    const auto supply = [](const std::filesystem::path& path) {
        std::filesystem::create_directories(path / "70/TABLE");
        std::filesystem::create_directories(path / "MATRIX/FONTS");
        std::ofstream(path / "70/TABLE/VIEW.BIT") << "synthetic";
        std::ofstream(path / "MATRIX/FONTS/FONT1.BIT") << "synthetic";
    };
    try {
        // No private artwork is used. Discovery never creates directories.
        check(!web::find_desktop_assets(executable, shared));
        check(!std::filesystem::exists(root));
        supply(executable / "data/PC_DAT");
        check(web::find_desktop_assets(executable, shared) == std::filesystem::canonical(executable / "data/PC_DAT"));
        supply(shared / "PC_DAT");
        check(web::find_desktop_assets(executable, shared) == std::filesystem::canonical(shared / "PC_DAT"));
        // Moving/updating either app or launching from another CWD leaves shared media usable.
        check(web::find_desktop_assets(root / "relocated app", shared) == std::filesystem::canonical(shared / "PC_DAT"));
        std::filesystem::remove(shared / "PC_DAT/MATRIX/FONTS/FONT1.BIT");
        check(web::find_desktop_assets(executable, shared) == std::filesystem::canonical(executable / "data/PC_DAT"));
        // Preserve all three existing portable/development layouts.
        check(web::find_desktop_assets(executable) == std::filesystem::canonical(executable / "data/PC_DAT"));
        std::filesystem::remove_all(executable / "data");
        supply(executable / "PC_DAT");
        check(web::find_desktop_assets(executable) == std::filesystem::canonical(executable / "PC_DAT"));
        std::filesystem::remove_all(executable / "PC_DAT");
        supply(root / "data/PC_DAT");
        check(web::find_desktop_assets(executable) == std::filesystem::canonical(root / "data/PC_DAT"));
        std::filesystem::remove_all(root);
        std::cout << "PASS: shared and portable asset discovery\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        std::filesystem::remove_all(root);
        return 1;
    }
}
