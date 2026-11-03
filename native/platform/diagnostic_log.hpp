#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>

namespace web {
// Host-only reporting: failures never throw back into the game or save path.
// One bounded log per edition and a short notice for the active window.
class DiagnosticLog {
public:
    static constexpr std::uintmax_t maximum_bytes = 64 * 1024;
    void directory(const std::filesystem::path& value) {
        file_ = value / "session.log";
    }
    bool report(const std::string& category, const std::string& detail) noexcept {
        try {
            if (reported_[category] == detail)
                return false;
            reported_[category] = detail;
            notice_ = category;
            visible_until_ = std::chrono::steady_clock::now() + std::chrono::seconds(12);
            std::cerr << category << ": " << detail << '\n';
            if (!file_.empty()) {
                std::error_code error;
                std::filesystem::create_directories(file_.parent_path(), error);
                if (!error) {
                    const auto size = std::filesystem::file_size(file_, error);
                    const auto mode = !error && size >= maximum_bytes / 2 ? std::ios::trunc : std::ios::app;
                    std::ofstream output(file_, mode);
                    const auto seconds = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
                    output << seconds << ' ' << category << ": " << detail.substr(0, maximum_bytes / 4) << '\n';
                }
            }
            return true;
        } catch (...) {
            return false;
        }
    }
    void clear(const std::string& category) {
        reported_.erase(category);
        if (notice_ == category)
            notice_.clear();
    }
    std::string notice() const {
        return std::chrono::steady_clock::now() < visible_until_ ? notice_ : std::string{};
    }
    const std::filesystem::path& file() const {
        return file_;
    }

private:
    std::filesystem::path file_;
    std::map<std::string, std::string> reported_;
    std::string notice_;
    std::chrono::steady_clock::time_point visible_until_{};
};
} // namespace web
