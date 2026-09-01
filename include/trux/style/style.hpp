#pragma once

#include <cstdint>

namespace trux::style {
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

constexpr Style operator|(Style lhs, Style rhs) {
    return static_cast<Style>(static_cast<uint8_t>(lhs) |
                              static_cast<uint8_t>(rhs));
}

constexpr Style& operator|=(Style& lhs, Style rhs) {
    lhs = lhs | rhs;
    return lhs;
}

constexpr Style operator&(Style lhs, Style rhs) {
    return static_cast<Style>(static_cast<uint8_t>(lhs) &
                              static_cast<uint8_t>(rhs));
}

constexpr bool has(Style value, Style flag) {
    return static_cast<uint8_t>(value & flag) != 0;
}

}  // namespace trux::style
