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

    Unknown,
};
}  // namespace trux::input
