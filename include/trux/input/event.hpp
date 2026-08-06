#pragma once

#include "trux/input/mouse.hpp"
#include "trux/layout/size.hpp"

#include <concepts>
#include <string>
#include <trux/input/key.hpp>
#include <trux/input/modifiers.hpp>

namespace trux::input {

enum class EventKind : uint8_t { Key, Mouse, Paste, Resize, Async, Quit };

struct Event {
    EventKind    kind{EventKind::Key};
    char32_t     code{};
    Modifiers    mods{};
    bool         valid{false};
    MouseEvent   mouse{};
    std::string  paste{};
    layout::Size resize{};
    bool         consumed{false};

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
    static Event from_paste(std::string text) noexcept {
        return Event{
            .kind = EventKind::Paste, .valid = true, .paste = std::move(text)};
    }
    [[nodiscard]]
    static constexpr Event from_resize(layout::Size size) noexcept {
        return Event{.kind = EventKind::Resize, .valid = true, .resize = size};
    }
    [[nodiscard]]
    static constexpr Event from_async(int fd) noexcept {
        return Event{
            .kind  = EventKind::Async,
            .code  = static_cast<char32_t>(fd),
            .valid = true,
        };
    }
    [[nodiscard]]
    static constexpr Event quit() {
        return Event{.kind = EventKind::Quit, .valid = true};
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
