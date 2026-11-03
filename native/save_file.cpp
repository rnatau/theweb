#include "save_file.hpp"
#include <stdexcept>
#include <utility>

namespace web {
SaveFile::SaveFile(std::filesystem::path directory, std::string name, std::size_t size, Validator validate, std::unique_ptr<SaveStorage> storage) :
    directory_(std::move(directory)),
    name_(std::move(name)),
    size_(size),
    validate_(std::move(validate)),
    storage_(std::move(storage)) {
    if (!storage_)
        throw std::invalid_argument("Save storage is required");
    if (name_.empty() || std::filesystem::path(name_).filename() != name_ || !size_)
        throw std::invalid_argument("Invalid application save filename or size");
}
SaveFile::~SaveFile() = default;
std::optional<Bytes> SaveFile::load() {
    if (loaded_)
        throw std::logic_error("Save file already loaded");
    try {
        std::filesystem::create_directories(directory_);
        storage_->lock(directory_ / (name_ + ".lock"));
        loaded_ = true;
        const auto path = directory_ / name_;
        if (!std::filesystem::exists(path)) {
            writable_ = true;
            return {};
        }
        if (std::filesystem::file_size(path) != size_)
            throw std::runtime_error("Save file has an invalid size");
        auto bytes = read_file(path);
        validate_(bytes);
        previous_ = bytes;
        writable_ = true;
        return bytes;
    } catch (const std::exception& error) {
        error_ = error.what();
        return {};
    }
}
bool SaveFile::externally_changed(const std::filesystem::path& target) const {
    if (!previous_)
        return std::filesystem::exists(target);
    if (!std::filesystem::exists(target))
        return true;
    return std::filesystem::file_size(target) != previous_->size() || read_file(target) != *previous_;
}
bool SaveFile::save(Bytes bytes) {
    if (!writable_)
        return false;
    try {
        if (bytes.size() != size_)
            throw std::runtime_error("Save file has an invalid size");
        validate_(bytes);
        const auto target = directory_ / name_;
        if (externally_changed(target))
            throw std::runtime_error("Save file changed outside this application");
        if (previous_ && *previous_ == bytes) {
            error_.clear();
            return true;
        }
        storage_->replace(target, bytes);
        previous_ = std::move(bytes);
        error_.clear();
        return true;
    } catch (const std::exception& error) {
        error_ = error.what();
        return false;
    }
}
} // namespace web
