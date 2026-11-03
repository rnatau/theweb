#include "save_storage.hpp"
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#include <system_error>

namespace web {
namespace {
void failure(const char* operation) {
    throw std::system_error(errno, std::generic_category(), operation);
}
class File {
public:
    int fd = -1;
    File() = default;
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    ~File() {
        if (fd >= 0)
            ::close(fd);
    }
};
class PosixSaveStorage final : public SaveStorage {
public:
    void lock(const std::filesystem::path& path) override {
        if (lock_.fd >= 0)
            throw std::logic_error("Save file already loaded");
        File candidate;
        candidate.fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (candidate.fd < 0 || ::flock(candidate.fd, LOCK_EX | LOCK_NB) < 0)
            failure("Cannot lock save file");
        lock_.fd = candidate.fd;
        candidate.fd = -1;
    }
    void replace(const std::filesystem::path& target, std::span<const std::uint8_t> bytes) override {
        if (lock_.fd < 0)
            throw std::logic_error("Save file is not locked");
        auto temporary = (target.parent_path() / (target.filename().string() + ".tmp-XXXXXX")).string();
        File file;
        file.fd = ::mkstemp(temporary.data());
        if (file.fd < 0)
            failure("Cannot create temporary save file");
        try {
            std::size_t offset = 0;
            while (offset < bytes.size()) {
                const auto count = ::write(file.fd, bytes.data() + offset, bytes.size() - offset);
                if (count < 0 && errno == EINTR)
                    continue;
                if (count <= 0)
                    failure("Cannot write save file");
                offset += std::size_t(count);
            }
            if (::fsync(file.fd) < 0)
                failure("Cannot flush save file");
            if (::rename(temporary.c_str(), target.c_str()) < 0)
                failure("Cannot replace save file");
            temporary.clear();
            File directory;
            directory.fd = ::open(target.parent_path().c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
            if (directory.fd < 0 || ::fsync(directory.fd) < 0)
                failure("Cannot flush save directory");
        } catch (...) {
            if (!temporary.empty())
                ::unlink(temporary.c_str());
            throw;
        }
    }

private:
    File lock_;
};
} // namespace
std::unique_ptr<SaveStorage> make_posix_save_storage() {
    return std::make_unique<PosixSaveStorage>();
}
} // namespace web
