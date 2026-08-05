#pragma once

#include <compare>
#include <cstdint>
#include <trux/input/modifiers.hpp>
#include <trux/layout/position.hpp>

namespace trux::input {

enum class MouseButton : uint8_t { Left, Middle, Right, None };
enum class MouseKind : uint8_t { Press, Release, Drag, ScrollUp, ScrollDown };

struct MouseEvent {
    layout::Position position{};
    MouseButton      button{MouseButton::None};
    MouseKind        kind{MouseKind::Press};
    Modifiers        mods{};

    constexpr auto operator<=>(const MouseEvent&) const = default;
};
}  // namespace trux::input
