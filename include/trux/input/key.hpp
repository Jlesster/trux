/// @file key.hpp
/// @brief Key codes for non-printable/special keys, encoded above the
///        Unicode range so they can share a char32_t with ordinary
///        codepoints (see Event::operator char32_t()).

#pragma once

#include <cstdint>
namespace trux::input {

/// First codepoint reserved for special keys. Ordinary Unicode
/// codepoints (including all valid UTF-8 input) fall below this
/// value, so a char32_t can hold either a printable character or one
/// of the Key values below without ambiguity.
inline constexpr char32_t KeyBackSpace = 0x110000;

/// Whether a key event is an initial press, an auto-repeat, or a release.
enum class KeyState : uint8_t { Press, Repeat, Release };

/// Special (non-printable) keys, encoded as codepoints starting at
/// KeyBackSpace so they can be stored in the same char32_t field as
/// printable characters.
enum Key : char32_t {
    Escape = KeyBackSpace,
    Enter,
    Backspace,
    Tab,

    Up,
    Down,
    Left,
    Right,

    Home,
    End,
    PageUp,
    PageDown,
    Delete,
    Insert,

    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,

    /// Sentinel for a key that couldn't be decoded.
    Unknown,
};
}  // namespace trux::input
