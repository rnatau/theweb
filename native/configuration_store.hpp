#pragma once
#include "options.hpp"
#include "save_file.hpp"

namespace web {
class ConfigurationStore {
public:
    ConfigurationStore(std::filesystem::path, std::unique_ptr<SaveStorage>);
    std::optional<GameOptions> load();
    bool save(const GameOptions&);
    const std::string& error() const {
        return file_.error();
    }

private:
    SaveFile file_;
};
} // namespace web
