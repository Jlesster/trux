/// @file event.hpp
/// @brief Event: the tagged-union type carrying every kind of input
///        (keyboard, mouse, paste, resize, etc.) that flows through
///        trux, plus the Handleable concept components use to opt
///        into receiving it.

#pragma once

#include "trux/input/mouse.hpp"
#include "trux/layout/size.hpp"

#include <concepts>
#include <string>
#include <trux/input/key.hpp>
#include <trux/input/modifiers.hpp>

namespace trux::input {

/// Which member of Event is meaningful, discriminating the union-like
/// fields (mouse, paste, resize) and giving Async/Quit/Tick events
/// with no extra payload.
enum class EventKind : uint8_t { Key, Mouse, Paste, Resize, Async, Quit, Tick };

/// A single input event. Only the fields relevant to `kind` are
/// populated; construct instances via the named factory functions
/// (key(), from_mouse(), etc.) rather than aggregate-initializing
/// directly.
struct Event {
    /// Which kind of event this is; determines which other fields are relevant.
    EventKind    kind{EventKind::Key};
    /// For Key events: the key's codepoint (see Key for special keys).
    char32_t     code{};
    /// Modifiers held at the time of the event.
    Modifiers    mods{};
    /// For Key events: whether this is a press, repeat, or release.
    KeyState     key_state{KeyState::Press};
    /// Whether this event represents real input (false for a
    /// default-constructed/none() event).
    bool         valid{false};
    /// Set by a handler to mark the event as already acted on.
    bool         consumed{false};
    /// For Mouse events: the mouse action details.
    MouseEvent   mouse{};
    /// For Paste events: the pasted text.
    std::string  paste{};
    /// For Resize events: the new terminal size.
    layout::Size resize{};

    /// Whether this is a real (valid) event.
    constexpr explicit operator bool() const noexcept { return valid; }
    /// Packs `code` and `mods` into a single char32_t, for use as a
    /// lookup key (e.g. in Keymap) that captures both the key and its
    /// modifiers.
    constexpr          operator char32_t() const noexcept {
        return code | (static_cast<char32_t>(mods.value) << 24);
    }

    /// Constructs a Key event for `code`, optionally with modifiers
    /// and a non-default KeyState.
    [[nodiscard]]
    static constexpr Event key(char32_t  code,
                               Modifiers mods  = {},
                               KeyState  state = KeyState::Press) noexcept {
        return Event{
            .code = code, .mods = mods, .key_state = state, .valid = true};
    }
    /// Constructs a Mouse event wrapping `m`.
    [[nodiscard]]
    static constexpr Event from_mouse(MouseEvent m) noexcept {
        return Event{.kind  = EventKind::Mouse,
                     .mods  = m.mods,
                     .valid = true,
                     .mouse = m};
    }
    /// Constructs a Paste event carrying `text`.
    [[nodiscard]]
    static Event from_paste(std::string text) noexcept {
        return Event{
            .kind = EventKind::Paste, .valid = true, .paste = std::move(text)};
    }
    /// Constructs a Resize event carrying the new terminal `size`.
    [[nodiscard]]
    static constexpr Event from_resize(layout::Size size) noexcept {
        return Event{.kind = EventKind::Resize, .valid = true, .resize = size};
    }
    /// Constructs an Async event, used to wake the event loop when
    /// activity is ready on file descriptor `fd` (e.g. an
    /// async::Channel or async::Executor). The fd is stored in `code`.
    [[nodiscard]]
    static constexpr Event from_async(int fd) noexcept {
        return Event{
            .kind  = EventKind::Async,
            .code  = static_cast<char32_t>(fd),
            .valid = true,
        };
    }
    /// Constructs a Quit event, signaling the event loop should stop.
    [[nodiscard]]
    static constexpr Event quit() {
        return Event{.kind = EventKind::Quit, .valid = true};
    }
    /// Constructs an invalid/empty event (valid == false).
    [[nodiscard]]
    static constexpr Event none() {
        return Event{};
    }
    /// Constructs a Tick event, used for periodic wakeups with no
    /// associated input.
    [[nodiscard]]
    static constexpr Event tick() noexcept {
        return Event{.kind = EventKind::Tick, .valid = true};
    }
};

/// Satisfied by any type exposing `bool handle(const Event&)`. Used
/// throughout trux (Renderer::push(), Split, Dialog, etc.) to detect
/// at compile time whether a component wants input dispatched to it.
template <typename T>
concept Handleable = requires(T& c, const Event& e) {
    { c.handle(e) } -> std::same_as<bool>;
};
}  // namespace trux::input
