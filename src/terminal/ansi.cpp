#include <print>
#include <trux/terminal/ansi.hpp>

void trux::ansi::hide_cursor() {
    std::print("\x1b[?25l");
    std::fflush(stdout);
}

void trux::ansi::show_cursor() {
    std::print("\x1b[?25h");
    std::fflush(stdout);
}

void trux::ansi::enter_alt_screen() {
    std::print("\x1b[?1049h");
    std::fflush(stdout);
}

void trux::ansi::leave_alt_screen() {
    std::print("\x1b[?1049l");
    std::fflush(stdout);
}

void trux::ansi::enable_kitty_keyboard() {
    std::print("\x1b[>1u");
    std::fflush(stdout);
}

void trux::ansi::disable_kitty_keyboard() {
    std::print("\x1b[>u");
    std::fflush(stdout);
}

void trux::ansi::enable_mouse() {
    std::print("\x1b[?1002h");
    std::print("\x1b[?1006h");
    std::fflush(stdout);
}

void trux::ansi::disable_mouse() {
    std::print("\x1b[?1006l");
    std::print("\x1b[?1002l");
    std::fflush(stdout);
}

void trux::ansi::clear() {
    std::print("\x1b[2J");
    std::fflush(stdout);
}

void trux::ansi::flush() { std::fflush(stdout); }
