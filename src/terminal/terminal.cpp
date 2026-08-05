#include "trux/input/event.hpp"
#include "trux/renderer/renderer.hpp"

#include <asm-generic/ioctls.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

#include <atomic>
#include <csignal>
#include <expected>
#include <memory>
#include <optional>
#include <print>
#include <trux/terminal/ansi.hpp>
#include <trux/terminal/terminal.hpp>

namespace {
std::atomic<bool> g_signal_requested{false};

extern "C" void handle_signal(int) {
    g_signal_requested.store(true, std::memory_order_relaxed);
}

std::string sgr_codes(const trux::renderer::Cell& cell) {
    using trux::style::Style;

    auto has = [style = static_cast<uint8_t>(cell.style)](Style s) {
        return style & static_cast<uint8_t>(s);
    };

    std::string codes = "0;";

    codes += std::format("38;2;{};{};{};48;2;{};{};{}",
                         cell.foreground.r,
                         cell.foreground.g,
                         cell.foreground.b,
                         cell.background.r,
                         cell.background.g,
                         cell.background.b);

    if(has(Style::Bold)) codes += ";1";
    if(has(Style::Dim)) codes += ";2";
    if(has(Style::Italic)) codes += ";3";
    if(has(Style::Underline)) codes += ";4";
    if(has(Style::Blink)) codes += ";5";
    if(has(Style::Reverse)) codes += ";7";
    if(has(Style::Strike)) codes += ";9";

    return codes;
}

std::string utf8_encode(char32_t cp) {
    std::string out;

    if(cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if(cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if(cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if(cp <= 0x10FFFF) {
        out += static_cast<char>(0xE0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

}  // namespace

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

std::expected<void, std::string> Terminal::enable_raw_mode() {
    // Terminal options struct
    termios raw = m_impl->original_termios;

    // setting terminal options using bitmask
    raw.c_lflag &= ~(ECHO | ICANON | ISIG);
    raw.c_iflag &= ~(IXON | ICRNL);
    raw.c_oflag &= ~(OPOST);

    raw.c_cc[VMIN]  = 1;
    raw.c_cc[VTIME] = 0;

    if(tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
        return std::unexpected("[ERROR] Failed to enable raw mode");

    m_impl->raw_enabled = true;
    return {};
}

std::expected<void, std::string> Terminal::init() {
    if(m_impl == nullptr)
        return std::unexpected("[ERROR] Invalid terminal state");
    if(!isatty(STDIN_FILENO))
        return std::unexpected("[ERROR] stdin is not a terminal");

    if(tcgetattr(STDIN_FILENO, &m_impl->original_termios) == -1)
        return std::unexpected("[ERROR] Failed to read terminal settings");

    // alternate screen
    ansi::enter_alt_screen();

    // enable kitty keyboard protocol
    ansi::enable_kitty_keyboard();

    // enable mouse support
    ansi::enable_mouse();

    // clear screen
    ansi::clear();

    // hide cursor
    ansi::hide_cursor();

    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);

    m_impl->initialized = true;
    return {};
}

bool Terminal::should_quit() const noexcept {
    return g_signal_requested.load(std::memory_order_relaxed);
}

void Terminal::shutdown() {
    if(m_impl == nullptr) return;
    if(!m_impl->initialized) return;

    // show cursor
    ansi::show_cursor();

    // disable mouse support
    ansi::disable_mouse();

    // disable kitty keyboard protocol
    ansi::disable_kitty_keyboard();

    // leave alternate screen
    ansi::leave_alt_screen();

    if(m_impl->raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_impl->original_termios);
    }

    m_impl->initialized = false;
}

layout::Size Terminal::size() const {
    winsize ws{};

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);

    return {.width  = static_cast<int>(ws.ws_col),
            .height = static_cast<int>(ws.ws_row)};
}

void Terminal::present(renderer::Renderer& renderer) {
    m_renderer = &renderer;

    for(const auto& batch : renderer.batches()) {
        if(batch.cells.empty()) continue;
        std::print("\x1b[{};{}H", batch.position.y + 1, batch.position.x + 1);

        // std::print("\x1b[0m");
        std::print("\x1b[{}m", sgr_codes(batch.cells.front()));

        for(const auto& cell : batch.cells) {
            std::print("{}", utf8_encode(cell.glyph));
        }
    }

    ansi::flush();
    renderer.commit();
}

bool Terminal::dispatch(const input::Event& event) const {
    if(!m_renderer) return false;
    return m_renderer->dispatch(event);
}

bool Terminal::has_pending(int timeout) const {
    pollfd pfd{STDIN_FILENO, POLLIN, 0};
    return ::poll(&pfd, 1, timeout) > 0;
}

}  // namespace trux
