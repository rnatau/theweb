#pragma once
#include "web.hpp"
#include "save_file.hpp"
#include <optional>
#include <string>

namespace web {
// Application-owned disk boundary. The deterministic game never opens files.
class ScoreStore {
public:
    ScoreStore(std::filesystem::path directory, std::unique_ptr<SaveStorage> storage);
    ScoreStore(const ScoreStore&) = delete;
    ScoreStore& operator=(const ScoreStore&) = delete;
    std::optional<Scores> load();
    bool save(const Scores&);
    const std::string& error() const {
        return file_.error();
    }

private:
    SaveFile file_;
};
// Reject checksum-correct records that cannot safely enter the live renderer.
// The raw original codec and typed mapping deliberately remain lossless.
void validate_playable_scores(const Scores&);
} // namespace web
