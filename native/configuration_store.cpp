#include "configuration_store.hpp"
#include <utility>

namespace web {
ConfigurationStore::ConfigurationStore(std::filesystem::path path, std::unique_ptr<SaveStorage> storage) :
    file_(std::move(path), "wgame.cfg", configuration_record_bytes, [](std::span<const std::uint8_t> bytes) { GameOptions::validate(parse_configuration(bytes)); }, std::move(storage)) {}
std::optional<GameOptions> ConfigurationStore::load() {
    if (auto bytes = file_.load())
        return GameOptions(parse_configuration(*bytes));
    return {};
}
bool ConfigurationStore::save(const GameOptions& options) {
    return file_.save(serialize_configuration(options.configuration()));
}
} // namespace web
