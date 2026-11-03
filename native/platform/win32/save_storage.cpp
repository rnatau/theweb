#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "save_storage.hpp"
#include <system_error>

namespace web {
namespace {
class File {
public:
    File() = default;
    File(const File&) = delete;
    File& operator=(const File&) = delete;
    HANDLE handle = INVALID_HANDLE_VALUE;
    ~File() {
        close();
    }
    void close() {
        if (handle != INVALID_HANDLE_VALUE) {
            CloseHandle(handle);
            handle = INVALID_HANDLE_VALUE;
        }
    }
};
void failure(const char* operation) {
    throw std::system_error(int(GetLastError()), std::system_category(), operation);
}
class Win32SaveStorage final : public SaveStorage {
public:
    void lock(const std::filesystem::path& path) override {
        if (lock_.handle != INVALID_HANDLE_VALUE)
            throw std::logic_error("Save file already loaded");
        lock_.handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (lock_.handle == INVALID_HANDLE_VALUE)
            failure("Cannot lock save file");
    }
    void replace(const std::filesystem::path& target, std::span<const std::uint8_t> bytes) override {
        if (lock_.handle == INVALID_HANDLE_VALUE)
            throw std::logic_error("Save file is not locked");
        std::filesystem::path temporary;
        bool owns_temporary = false;
        try {
            File file;
            for (unsigned attempt = 0; attempt < 100; ++attempt) {
                temporary = target.parent_path() / (target.filename().string() + ".tmp-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(attempt));
                file.handle = CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
                if (file.handle != INVALID_HANDLE_VALUE)
                    break;
                if (GetLastError() != ERROR_FILE_EXISTS)
                    failure("Cannot create temporary save file");
                temporary.clear();
            }
            if (file.handle == INVALID_HANDLE_VALUE)
                throw std::runtime_error("Cannot reserve temporary save file");
            owns_temporary = true;
            DWORD written = 0;
            if (!WriteFile(file.handle, bytes.data(), DWORD(bytes.size()), &written, nullptr) || written != bytes.size())
                failure("Cannot write save file");
            if (!FlushFileBuffers(file.handle))
                failure("Cannot flush save file");
            file.close();
            if (!MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
                failure("Cannot replace save file");
        } catch (...) {
            if (owns_temporary) {
                std::error_code ignored;
                std::filesystem::remove(temporary, ignored);
            }
            throw;
        }
    }

private:
    File lock_;
};
} // namespace
std::unique_ptr<SaveStorage> make_win32_save_storage() {
    return std::make_unique<Win32SaveStorage>();
}
} // namespace web
