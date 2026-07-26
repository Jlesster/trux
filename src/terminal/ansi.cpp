#include <print>
#include <terminal/ansi.hpp>

void ansi::hide_cursor() {
    std::print("\x1b[?25l");
    std::fflush(stdout);
}

void ansi::show_cursor() {
    std::print("\x1b[?25h");
    std::fflush(stdout);
}

void ansi::enter_alt_screen() {
    std::print("\x1b[?1049h");
    std::fflush(stdout);
}

void ansi::leave_alt_screen() {
    std::print("\x1b[?1049l");
    std::fflush(stdout);
}

void ansi::clear() {
    std::print("\x1b[2J");
    std::fflush(stdout);
}

void ansi::flush() { std::fflush(stdout); }
