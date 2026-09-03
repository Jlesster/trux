/// @file style.hpp
/// @brief Bitmask enum for text attributes (bold, italic, etc.) and
///        its bitwise operators.

#pragma once

#include <cstdint>

namespace trux::style {

/// Bitmask of terminal text attributes. Combine values with
/// `operator|` and test with has().
///
/// @code{.cpp}
/// Style s = Style::Bold | Style::Underline;
/// if (has(s, Style::Bold)) { ... }
/// @endcode
enum class Style : uint8_t {
    None      = 0,
    Bold      = 1 << 0,
    Italic    = 1 << 1,
    Underline = 1 << 2,
    Blink     = 1 << 3,
    Reverse   = 1 << 4,
    Dim       = 1 << 5,
    Strike    = 1 << 6,
};

/// Combines two style flags.
constexpr Style operator|(Style lhs, Style rhs) {
    return static_cast<Style>(static_cast<uint8_t>(lhs) |
                              static_cast<uint8_t>(rhs));
}

/// Combines `rhs` into `lhs` in place.
constexpr Style& operator|=(Style& lhs, Style rhs) {
    lhs = lhs | rhs;
    return lhs;
}

/// Intersects two style flags (bitwise AND). Typically used together
/// with has() rather than called directly.
constexpr Style operator&(Style lhs, Style rhs) {
    return static_cast<Style>(static_cast<uint8_t>(lhs) &
                              static_cast<uint8_t>(rhs));
}

/// Whether `flag` is set within `value`.
///
/// @param value Combined style bitmask to test.
/// @param flag  Single flag to check for.
constexpr bool has(Style value, Style flag) {
    return static_cast<uint8_t>(value & flag) != 0;
}

}  // namespace trux::style
