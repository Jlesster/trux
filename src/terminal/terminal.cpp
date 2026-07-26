#include <asm-generic/ioctls.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <expected>
#include <iostream>
#include <memory>
#include <terminal/ansi.hpp>
#include <terminal/terminal.hpp>

namespace trux {
struct Terminal::Impl {
    termios original_termios{};
    bool    raw_enabled = false;
    bool    initialized = false;
};

Terminal::Terminal() : m_impl(std::make_unique<Impl>()) {}

Terminal::~Terminal() { shutdown(); }

Terminal::Terminal(Terminal&& other) noexcept
    : m_impl(std::move(other.m_impl)) {}

Terminal& Terminal::operator=(Terminal&& other) noexcept {
    if(this == &other) return *this;

    shutdown();

    m_impl = std::move(other.m_impl);

    return *this;
}

std::expected<void, std::string> Terminal::init() {
    if(m_impl == nullptr)
        return std::unexpected("[ERROR] Invalid terminal state");
    if(!isatty(STDIN_FILENO))
        return std::unexpected("[ERROR] stdin is not a terminal");

    if(tcgetattr(STDIN_FILENO, &m_impl->original_termios) == -1)
        return std::unexpected("[ERROR] Failed to read terminal settings");

    termios raw = m_impl->original_termios;

    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        return std::unexpected("[ERROR] Failed to enable raw mode");

    m_impl->raw_enabled = true;

    // alternate screen
    ansi::enter_alt_screen();

    // hide cursor
    ansi::hide_cursor();

    m_impl->initialized = true;
    return {};
}

void Terminal::shutdown() {
    if(m_impl == nullptr) return;
    if(!m_impl->initialized) return;

    // show cursor
    ansi::show_cursor();

    // leave alternate screen
    ansi::leave_alt_screen();

    if(m_impl->raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_impl->original_termios);
    }

    m_impl->initialized = false;
}

Size Terminal::size() const {
    winsize ws{};

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    return {.width  = static_cast<int>(ws.ws_col),
            .height = static_cast<int>(ws.ws_row)};
}

void Terminal::clear() { ansi::clear(); }

void Terminal::present() { ansi::flush(); }

}  // namespace trux
