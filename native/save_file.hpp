#pragma once
#include "save_storage.hpp"
#include <functional>
#include <optional>

namespace web {
// Atomic, exclusively locked application data outside the reference media.
class SaveFile {
public:
    using Validator = std::function<void(std::span<const std::uint8_t>)>;
    SaveFile(std::filesystem::path directory, std::string name, std::size_t size, Validator validate, std::unique_ptr<SaveStorage> storage);
    ~SaveFile();
    SaveFile(const SaveFile&) = delete;
    SaveFile& operator=(const SaveFile&) = delete;
    std::optional<Bytes> load();
    bool save(Bytes);
    const std::string& error() const {
        return error_;
    }

private:
    bool externally_changed(const std::filesystem::path& target) const;
    std::filesystem::path directory_;
    std::string name_;
    std::size_t size_;
    Validator validate_;
    std::unique_ptr<SaveStorage> storage_;
    bool loaded_ = false;
    bool writable_ = false;
    std::optional<Bytes> previous_;
    std::string error_;
};
} // namespace web
