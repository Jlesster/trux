/// @file modifiers.hpp
/// @brief Keyboard modifier flags and helpers for combining them with
///        each other and with key codes.

#pragma once

#include <compare>
#include <cstdint>

namespace trux::input {

/// A single keyboard modifier, as a distinct bit.
enum class Mod : uint8_t {
    None  = 0,
    Shift = 1 << 0,
    Alt   = 1 << 1,
    Ctrl  = 1 << 2,
    Super = 1 << 3,
};

/// A bitset of zero or more simultaneously-held Mod values.
struct Modifiers {
    uint8_t value{};

    /// Sets modifier `m`.
    constexpr void add(Mod m) noexcept { value |= static_cast<uint8_t>(m); }
    /// Whether modifier `m` is set.
    [[nodiscard]]
    constexpr bool has(Mod m) const noexcept {
        return value & static_cast<uint8_t>(m);
    }
    constexpr auto operator<=>(const Modifiers&) const = default;
};

/// Decodes the modifier parameter of a CSI escape sequence (as used
/// by xterm-style extended key/mouse reporting) into a Modifiers set.
/// `param` follows the ANSI convention of being one greater than the
/// modifier bitmask, with 0 or 1 meaning "no modifiers".
[[nodiscard]]
constexpr Modifiers decode_modifier_param(int param) noexcept {
    Modifiers m{};
    if(param <= 1) return m;
    int bits = param - 1;
    if(bits & 1) m.add(Mod::Shift);
    if(bits & 2) m.add(Mod::Alt);
    if(bits & 4) m.add(Mod::Ctrl);
    if(bits & 8) m.add(Mod::Super);
    return m;
}

/// Wraps a single Mod in a Modifiers set.
[[nodiscard]]
constexpr Modifiers with(Mod m) noexcept {
    Modifiers mods{};
    mods.add(m);
    return mods;
}

/// Combines two modifiers into a set.
[[nodiscard]]
constexpr Modifiers operator|(Mod lhs, Mod rhs) noexcept {
    Modifiers m = with(lhs);
    m.add(rhs);
    return m;
}

/// Adds `rhs` to an existing Modifiers set.
[[nodiscard]]
constexpr Modifiers operator|(Modifiers lhs, Mod rhs) noexcept {
    lhs.add(rhs);
    return lhs;
}

/// Packs `mod` into the high byte of `code`, producing the combined
/// char32_t representation used by Event::operator char32_t().
[[nodiscard]]
constexpr char32_t operator|(char32_t code, Mod mod) noexcept {
    return code | (static_cast<char32_t>(with(mod).value) << 24);
}

/// Packs `mods` into the high byte of `code`, producing the combined
/// char32_t representation used by Event::operator char32_t().
[[nodiscard]]
constexpr char32_t operator|(char32_t code, Modifiers mods) noexcept {
    return code | (static_cast<char32_t>(mods.value) << 24);
}

}  // namespace trux::input
