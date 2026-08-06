#include "trux/input/event.hpp"
#include "trux/renderer/renderer.hpp"
#include "trux/util/util.hpp"

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
std::atomic<bool> g_resize_requested{false};

extern "C" void handle_signal(int) {
    g_signal_requested.store(true, std::memory_order_relaxed);
}
extern "C" void handle_resize(int) {
    g_resize_requested.store(true, std::memory_order_relaxed);
}

void install_handler(int sig, void (*handler)(int)) {
    struct sigaction sa{};
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(sig, &sa, nullptr);
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
    if(!m_impl || !m_impl->initialized)
        return std::unexpected("[ERROR] enable_raw_mode called before init");

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

    // bracketed paste
    ansi::enable_bracketed_paste();

    // enable mouse support
    ansi::enable_mouse();

    // clear screen
    ansi::clear();

    // hide cursor
    ansi::hide_cursor();

    install_handler(SIGINT, handle_signal);
    install_handler(SIGTERM, handle_signal);
    install_handler(SIGWINCH, handle_resize);

    m_impl->initialized = true;
    return {};
}

bool Terminal::should_quit() const noexcept {
    return g_signal_requested.load(std::memory_order_relaxed);
}

void Terminal::request_quit() noexcept {
    g_signal_requested.store(true, std::memory_order_relaxed);
}

void Terminal::shutdown() {
    if(m_impl == nullptr) return;
    if(!m_impl->initialized) return;

    // show cursor
    ansi::show_cursor();

    // disable mouse support
    ansi::disable_mouse();

    // bracketed paste
    ansi::disable_bracketed_paste();

    // disable kitty keyboard protocol
    ansi::disable_kitty_keyboard();

    // leave alternate screen
    ansi::leave_alt_screen();

    if(m_impl->raw_enabled) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &m_impl->original_termios);
        m_impl->raw_enabled = false;
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
        std::print("\x1b[{}m", util::sgr_codes(batch.cells.front()));

        for(const auto& cell : batch.cells) {
            std::print("{}", util::encode_utf8(cell.glyph));
        }
    }

    ansi::flush();
    renderer.commit();
}

bool Terminal::wait_readable(int                  primary_fd,
                             std::span<const int> extra_fds,
                             int                  timeout_ms) {
    std::vector<pollfd> fds;
    fds.push_back({primary_fd, POLLIN, 0});
    for(int fd : extra_fds) fds.push_back({fd, POLLIN, 0});

    int result = ::poll(fds.data(), fds.size(), timeout_ms);
    if(result <= 0) {
        m_last_ready_fd.reset();
        return false;
    }

    for(const auto& pfd : fds) {
        if(pfd.revents & POLLIN) {
            m_last_ready_fd = pfd.fd;
            return true;
        }
    }

    m_last_ready_fd.reset();
    return false;
}

std::optional<char> Terminal::read() {
    char byte{};

    auto result = ::read(STDIN_FILENO, &byte, 1);

    if(result <= 0) return std::nullopt;

    return byte;
}

std::optional<int> Terminal::last_ready_fd() const noexcept {
    return m_last_ready_fd;
}

bool Terminal::dispatch(const input::Event& event) const {
    if(!m_renderer) return false;
    return m_renderer->dispatch(event);
}

bool Terminal::has_pending(int timeout) const {
    pollfd pfd{STDIN_FILENO, POLLIN, 0};
    return ::poll(&pfd, 1, timeout) > 0;
}

bool Terminal::resized() const noexcept {
    return g_resize_requested.exchange(false, std::memory_order_relaxed);
}

}  // namespace trux
