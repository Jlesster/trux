#pragma once

#include <cstdint>

namespace trux::component {

enum class Flag : uint64_t {
    None = 0,

    BorderSingle  = 1ULL << 0,
    BorderRounded = 1ULL << 1,
    BorderDouble  = 1ULL << 2,
    BorderBlock   = 1ULL << 3,

    Bold      = 1ULL << 4,
    Italic    = 1ULL << 5,
    Underline = 1ULL << 6,
};

struct ComponentFlags {
    uint64_t value{};

    constexpr void add(Flag flag) noexcept {
        value |= static_cast<uint64_t>(flag);
    }

    constexpr void remove(Flag flag) noexcept {
        value &= ~static_cast<uint64_t>(flag);
    }

    [[nodiscard]]
    constexpr bool has(Flag flag) const {
        return value & static_cast<uint64_t>(flag);
    }
};
constexpr Flag operator|(Flag lhs, Flag rhs) {
    return static_cast<Flag>(static_cast<uint64_t>(lhs) |
                             static_cast<uint64_t>(rhs));
}
}  // namespace trux::component
