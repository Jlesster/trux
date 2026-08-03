#pragma once

namespace trux::input {
inline constexpr char32_t KeyBackSpace = 0x110000;
enum Key : char32_t {
    Escape = KeyBackSpace,
    Enter,
    Backspace,
    Tab,

    Up,
    Down,
    Left,
    Right,

    Unknown,
};
}  // namespace trux::input
