#pragma once
#include <bit>
#include <cstdint>

namespace web::physics::integer {
// x86 ADD/SUB/IMUL keep the low 32 bits. Avoid signed C++ overflow.
constexpr std::int32_t bits(std::uint32_t value) {
    return std::bit_cast<std::int32_t>(value);
}
constexpr std::int32_t add(std::int32_t a, std::int32_t b) {
    return bits(std::uint32_t(a) + std::uint32_t(b));
}
constexpr std::int32_t sub(std::int32_t a, std::int32_t b) {
    return bits(std::uint32_t(a) - std::uint32_t(b));
}
constexpr std::int32_t mul(std::int32_t a, std::int32_t b) {
    return bits(std::uint32_t(a) * std::uint32_t(b));
}
} // namespace web::physics::integer
