/// @file mouse.hpp
/// @brief MouseEvent and the button/action enums used to describe it.

#pragma once

#include <compare>
#include <cstdint>
#include <trux/input/modifiers.hpp>
#include <trux/layout/position.hpp>

namespace trux::input {

/// Which mouse button an event refers to. `None` is used for
/// button-less actions such as scroll wheel events.
enum class MouseButton : uint8_t { Left, Middle, Right, None };

/// What kind of mouse action occurred.
enum class MouseKind : uint8_t { Press, Release, Drag, ScrollUp, ScrollDown };

/// A single mouse action: where it happened, which button (if any)
/// was involved, what kind of action it was, and any held modifiers.
struct MouseEvent {
    /// Cell the event occurred at.
    layout::Position position{};
    /// Button involved, or None for scroll events.
    MouseButton      button{MouseButton::None};
    /// Kind of action (press, release, drag, scroll).
    MouseKind        kind{MouseKind::Press};
    /// Keyboard modifiers held at the time of the event.
    Modifiers        mods{};

    constexpr auto operator<=>(const MouseEvent&) const = default;
};
}  // namespace trux::input
