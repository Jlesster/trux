/// @file terminal.hpp
/// @brief Terminal: owns the controlling terminal's raw-mode state
///        and lifecycle, and presents a Renderer's output to it.

#pragma once

#include "trux/layout/size.hpp"
#include "trux/renderer/renderer.hpp"

#include <expected>
#include <memory>
#include <optional>
#include <span>

namespace trux::input {
struct Event;
}

namespace trux {

/// RAII wrapper around the process's controlling terminal: enters/
/// restores terminal modes (alt screen, raw mode, mouse reporting,
/// etc.), reads raw input bytes, and presents a Renderer's frame
/// output.
///
/// Typical usage:
/// @code{.cpp}
/// Terminal term;
/// if (auto r = term.init(); !r) { // handle r.error() }
/// term.enable_raw_mode();
/// // ... event loop: read input, dispatch, renderer.begin_draw()/push()/
/// //     end_draw(), term.present(renderer) ...
/// // shutdown() runs automatically in the destructor, or call it early.
/// @endcode
///
/// Movable but not copyable — a Terminal owns process-global state
/// (signal handlers, saved termios settings) that must not be
/// duplicated.
class Terminal {
public:
    /// Constructs a Terminal in an uninitialized state. Call init()
    /// before using it.
    Terminal();

    /// Restores terminal state via shutdown() if still initialized.
    ~Terminal();

    Terminal(const Terminal&)            = delete;
    Terminal& operator=(const Terminal&) = delete;

    /// Transfers ownership of the terminal's state from `other`,
    /// leaving `other` empty.
    Terminal(Terminal&&) noexcept;
    /// Shuts down this terminal's own state (if any) before taking
    /// over `other`'s.
    Terminal& operator=(Terminal&&) noexcept;

    /// Verifies stdin is a TTY, saves the original termios settings,
    /// enters the alternate screen, enables the Kitty keyboard
    /// protocol/bracketed paste/mouse reporting, clears the screen,
    /// hides the cursor, and installs SIGINT/SIGTERM/SIGWINCH
    /// handlers. Must be called (and succeed) before any other
    /// method except the constructor/destructor.
    ///
    /// @return Nothing on success, or an error message if stdin
    ///         isn't a terminal or termios settings couldn't be read.
    [[nodiscard]]
    std::expected<void, std::string> init();

    /// Puts the terminal into raw mode (disabling echo, canonical
    /// input processing, and signal-generating control characters)
    /// so input can be read byte-by-byte. Must be called after a
    /// successful init().
    ///
    /// @return Nothing on success, or an error message if raw mode
    ///         could not be enabled or init() has not been called.
    std::expected<void, std::string> enable_raw_mode();

    /// Whether a quit has been requested — either via request_quit()
    /// or because SIGINT/SIGTERM was received. Intended to be
    /// polled once per iteration of the event loop.
    [[nodiscard]]
    bool should_quit() const noexcept;

    /// Programmatically requests that should_quit() start returning
    /// true, as if a termination signal had been received.
    void request_quit() noexcept;

    /// Restores the terminal to its pre-init() state: shows the
    /// cursor, disables mouse/bracketed-paste/Kitty-keyboard modes,
    /// leaves the alternate screen, and restores the original
    /// termios settings if raw mode was enabled. Safe to call
    /// multiple times or without a prior init(). Called automatically
    /// by the destructor.
    void shutdown();

    /// Queries the current terminal size in columns/rows via
    /// `ioctl(TIOCGWINSZ)`.
    [[nodiscard]]
    layout::Size size() const;

    /// Blocks (up to `timeout_ms`, or indefinitely if negative)
    /// until `primary_fd` or any of `extra_fds` becomes readable.
    ///
    /// @param primary_fd  Main file descriptor to watch (typically stdin).
    /// @param extra_fds   Additional descriptors to watch alongside it
    ///                    (e.g. a timer or async-executor wakeup fd).
    /// @param timeout_ms  Timeout in milliseconds; negative blocks
    ///                    indefinitely.
    /// @return true if some descriptor became readable, false on
    ///         timeout. On true, last_ready_fd() reports which one.
    [[nodiscard]]
    bool wait_readable(int                  primary_fd,
                       std::span<const int> extra_fds,
                       int                  timeout_ms);

    /// The file descriptor that was ready after the most recent
    /// wait_readable() call that returned true, or `std::nullopt` if
    /// the last call timed out (or none has been made yet).
    [[nodiscard]]
    std::optional<int> last_ready_fd() const noexcept;

    /// Writes `renderer`'s current render batches to the terminal
    /// (positioning the cursor and emitting SGR codes + glyphs for
    /// each batch), flushes output, and commits the renderer's
    /// frame. Also remembers `renderer` so that a subsequent
    /// dispatch() call can route input to it.
    ///
    /// @param renderer Renderer whose batches() to present. Must
    ///                 outlive any later dispatch() calls made
    ///                 through this Terminal, or be re-passed to
    ///                 present() again first.
    void present(renderer::Renderer& renderer);

    /// Forwards `event` to the Renderer most recently passed to
    /// present(), if any.
    ///
    /// @return Whatever the renderer's dispatch() returned, or false
    ///         if present() has not yet been called.
    [[nodiscard]]
    bool dispatch(const input::Event&) const;

    /// Reads a single raw byte from stdin, non-blocking beyond
    /// whatever the OS read() call itself blocks for.
    ///
    /// @return The byte read, or `std::nullopt` on EOF/error.
    [[nodiscard]]
    std::optional<char> read();

    /// Whether stdin has input available to read within `timeout_ms`.
    ///
    /// @param timeout_ms Milliseconds to wait; 0 (the default) polls
    ///                   without blocking.
    [[nodiscard]]
    bool has_pending(int timeout_ms = 0) const;

    /// Whether a SIGWINCH (terminal resize) has been received since
    /// the last call to resized(). Consumes the pending flag — each
    /// resize is reported exactly once.
    [[nodiscard]]
    bool resized() const noexcept;

private:
    /// Platform-specific state (saved termios, raw-mode flag, etc.),
    /// hidden behind a pointer so this header stays free of
    /// `<termios.h>`/POSIX includes.
    struct Impl;
    std::unique_ptr<Impl> m_impl;
    std::optional<int>    m_last_ready_fd;
    renderer::Renderer*   m_renderer = nullptr;
};
}  // namespace trux
