/// @file component_flags.hpp
/// @brief Flag: the bitset of per-component styling toggles (border
///        style, bold/italic/underline) shared by every component
///        type, and the ComponentFlags bitset that holds them.

#pragma once

#include <cstdint>

namespace trux::component {

/// A single component styling toggle, as a distinct bit. Border
/// flags are mutually exclusive in practice (see
/// border::active_border()), but text style flags may be combined.
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

/// A bitset of zero or more simultaneously-set Flag values, embedded
/// in every component to control its border and text styling.
struct ComponentFlags {
    uint64_t value{};

    /// Sets `flag`.
    constexpr void add(Flag flag) noexcept {
        value |= static_cast<uint64_t>(flag);
    }

    /// Clears `flag`.
    constexpr void remove(Flag flag) noexcept {
        value &= ~static_cast<uint64_t>(flag);
    }

    /// Whether `flag` is set.
    [[nodiscard]]
    constexpr bool has(Flag flag) const {
        return value & static_cast<uint64_t>(flag);
    }
};

/// Combines two flags into a raw OR'd Flag value (not a
/// ComponentFlags). Mainly useful for building lookup keys, e.g. in
/// border::glyph_for_mask()'s direction masks.
constexpr Flag operator|(Flag lhs, Flag rhs) {
    return static_cast<Flag>(static_cast<uint64_t>(lhs) |
                             static_cast<uint64_t>(rhs));
}
}  // namespace trux::component
