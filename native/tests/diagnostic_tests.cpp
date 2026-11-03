#include "../platform/diagnostic_log.hpp"
#include <stdexcept>

int main() {
    const auto directory = std::filesystem::path("build") / "diagnostic-log-test";
    const auto file = directory / "session.log";
    web::DiagnosticLog log;
    if (!log.report("Sound unavailable", "no device") || log.report("Sound unavailable", "no device") || log.notice() != "Sound unavailable")
        throw std::runtime_error("Notice deduplication failed");
    log.clear("Sound unavailable");
    if (!log.notice().empty())
        throw std::runtime_error("Resolved notice remained visible");
    log.directory(directory);
    for (int i = 0; i < 12; ++i)
        log.report("Testing bounded log", std::to_string(i) + std::string(10000, 'x'));
    if (!std::filesystem::is_regular_file(file) || std::filesystem::file_size(file) > web::DiagnosticLog::maximum_bytes)
        throw std::runtime_error("Diagnostic log exceeded its size bound");
    std::filesystem::remove(file);
    std::filesystem::remove(directory);
}
