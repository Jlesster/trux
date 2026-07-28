#pragma once

#include <cstdint>

namespace trux::input {
enum class Key : uint8_t {
    Unknown,
    Escape,
    Enter,
    Backspace,
    Tab,

    Up,
    Down,
    Left,
    Right,

    Char,
};

struct KeyEvent {
    Key      key{Key::Unknown};
    char32_t character{};
};
}  // namespace trux::input
