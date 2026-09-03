/// @file ansi.hpp
/// @brief Low-level raw ANSI/VT escape sequence writers. Each
///        function writes directly to stdout and flushes.
///
/// These are the raw building blocks Terminal uses internally during
/// init()/shutdown(); most applications should drive Terminal
/// instead of calling these directly.

#pragma once

namespace trux::ansi {

/// Hides the terminal cursor (`\x1b[?25l`).
void hide_cursor();
/// Shows the terminal cursor (`\x1b[?25h`).
void show_cursor();

/// Switches to the terminal's alternate screen buffer, preserving
/// the user's existing scrollback/shell content underneath.
void enter_alt_screen();
/// Leaves the alternate screen buffer, restoring whatever was on
/// screen before enter_alt_screen().
void leave_alt_screen();

/// Enables the Kitty keyboard protocol, giving more precise key
/// event reporting (distinct press/release, modifier-only keys,
/// etc.) on terminals that support it.
void enable_kitty_keyboard();
/// Disables the Kitty keyboard protocol, reverting to legacy key reporting.
void disable_kitty_keyboard();

/// Enables SGR mouse reporting (button press/release/drag and
/// scroll events) via `\x1b[?1002h` + `\x1b[?1006h`.
void enable_mouse();
/// Disables mouse reporting.
void disable_mouse();

/// Enables bracketed paste mode, wrapping pasted input in escape
/// markers so it can be distinguished from typed input.
void enable_bracketed_paste();
/// Disables bracketed paste mode.
void disable_bracketed_paste();

/// Clears the entire visible screen.
void clear();
/// Flushes stdout. Other functions in this file already flush after
/// writing; call this directly only after writing raw output of
/// your own (e.g. in Terminal::present()).
void flush();
}  // namespace trux::ansi
