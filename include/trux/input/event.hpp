#pragma once

#include "trux/input/mouse.hpp"

#include <concepts>
#include <trux/input/key.hpp>
#include <trux/input/modifiers.hpp>

namespace trux::input {

enum class EventKind : uint8_t { Key, Mouse };

struct Event {
    EventKind  kind{EventKind::Key};
    char32_t   code{};
    Modifiers  mods{};
    bool       valid{false};
    MouseEvent mouse{};

    constexpr explicit operator bool() const noexcept { return valid; }
    constexpr          operator char32_t() const noexcept { return code; }

    [[nodiscard]]
    static constexpr Event key(char32_t code, Modifiers mods = {}) noexcept {
        return Event{.code = code, .mods = mods, .valid = true};
    }
    [[nodiscard]]
    static constexpr Event from_mouse(MouseEvent m) noexcept {
        return Event{.kind  = EventKind::Mouse,
                     .mods  = m.mods,
                     .valid = true,
                     .mouse = m};
    }
    [[nodiscard]]
    static constexpr Event none() {
        return Event{};
    }
};

template <typename T>
concept Handleable = requires(T& c, const Event& e) {
    { c.handle(e) } -> std::same_as<bool>;
};
}  // namespace trux::input
