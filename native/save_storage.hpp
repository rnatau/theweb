#pragma once
#include "web.hpp"
#include <memory>

namespace web {
// OS boundary for durable application saves. One instance owns one exclusive
// lock until destruction. Operations throw on failure; replace must preserve the
// old target on failure and clean up only temporary files it created.
class SaveStorage {
public:
    virtual ~SaveStorage() = default;
    virtual void lock(const std::filesystem::path& lock_file) = 0;
    virtual void replace(const std::filesystem::path& target, std::span<const std::uint8_t> bytes) = 0;
};
} // namespace web
