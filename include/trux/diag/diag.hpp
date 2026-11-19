/// @file diag.hpp
/// @brief Diagnostics: one place errors go instead of the terminal.
///
/// A TUI owns the screen, so printing errors to stderr garbles the UI
/// and a crash leaves the terminal in raw mode with mouse reporting
/// still on (which then spams escape codes on every mouse move). This
/// header provides:
///
///  - a log sink (file, ring buffer, repeat-collapsing) that components
///    report into: diag::warn()/diag::error()
///  - TRUX_ASSERT: an assert that says *what* was wrong, with values
///  - crash handling: terminate handler + fatal signal handlers that
///    restore the terminal first, then print a short report and write
///    the full backtrace to the log
///
/// Header-only. Nothing here touches the terminal directly; Terminal
/// registers a restore hook (see set_restore_hook()).
///
/// Environment:
///   TRUX_LOG=/path/file   log destination (default /tmp/trux-<pid>.log)
///   TRUX_STDERR=tty       don't redirect stderr into the log

#pragma once

#include <cxxabi.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <format>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <typeinfo>
#include <vector>

namespace trux::diag {

enum class Severity : uint8_t { Info, Warn, Error, Fatal };

[[nodiscard]]
constexpr std::string_view label(Severity s) noexcept {
    switch(s) {
        case Severity::Info: return "INFO";
        case Severity::Warn: return "WARN";
        case Severity::Error: return "ERROR";
        case Severity::Fatal: return "FATAL";
    }
    return "?";
}

/// One distinct diagnostic. Identical repeats (same severity, source
/// and message) are folded into `count` instead of new entries.
struct Diagnostic {
    Severity severity{Severity::Info};
    std::string source;    ///< component / subsystem, e.g. "Dropdown"
    std::string message;   ///< what went wrong
    std::string hint;      ///< optional: what to do about it
    std::string where;     ///< file:line of the report site
    std::chrono::steady_clock::time_point last{};
    unsigned count{1};
};

/// Demangles an Itanium-ABI symbol/type name; returns the input on failure.
[[nodiscard]]
inline std::string demangle(const char* mangled) {
    int status = 0;
    char* d = abi::__cxa_demangle(mangled, nullptr, nullptr, &status);
    std::string out = (status == 0 && d) ? d : mangled;
    std::free(d);
    return out;
}

/// Readable type name for diagnostics: demangled, with trux
/// namespaces and the ComponentWrapper<> type-erasure layer removed.
[[nodiscard]]
inline std::string short_type_name(const std::type_info& t) {
    std::string n = demangle(t.name());
    constexpr std::string_view wrap = "trux::component::ComponentWrapper<";
    if(n.starts_with(wrap) && n.ends_with('>'))
        n = n.substr(wrap.size(), n.size() - wrap.size() - 1);
    for(std::string_view ns : {"trux::component::",
                               "trux::layout::",
                               "trux::renderer::",
                               "trux::"}) {
        for(size_t p; (p = n.find(ns)) != std::string::npos;)
            n.erase(p, ns.size());
    }
    return n;
}

template <typename T>
[[nodiscard]]
std::string type_name() {
    return short_type_name(typeid(T));
}

/// "Type: what()" for the exception currently being handled. Call only
/// from inside a catch block (or terminate handler).
[[nodiscard]]
inline std::string describe_current_exception() {
    try {
        throw;
    } catch(const std::exception& e) {
        return demangle(typeid(e).name()) + ": " + e.what();
    } catch(...) {
        return "unknown (non-std) exception";
    }
}

class Log {
public:
    static Log& get() {
        static Log log;
        return log;
    }

    /// Records a diagnostic: ring buffer, counters, and the log file.
    /// Never writes to the terminal.
    void report(Severity sev, std::string_view source, std::string_view msg,
                std::string_view hint = {},
                std::source_location loc = std::source_location::current()) {
        std::lock_guard lock(m_mutex);
        const auto now = std::chrono::steady_clock::now();
        m_totals[static_cast<size_t>(sev)]++;

        for(auto it = m_ring.rbegin(); it != m_ring.rend(); ++it) {
            if(it->severity == sev && it->source == source &&
               it->message == msg) {
                it->count++;
                it->last = now;
                // Log the 2nd, 10th, 100th... repeat only.
                if(it->count == 2 || it->count == 10 || it->count == 100 ||
                   it->count == 1000)
                    write_line(std::format("  ^ repeated {}x: {}: {}",
                                           it->count, it->source, it->message));
                return;
            }
        }

        Diagnostic d;
        d.severity = sev;
        d.source = source;
        d.message = msg;
        d.hint = hint;
        d.where = std::format("{}:{}", short_file(loc.file_name()), loc.line());
        d.last = now;
        write_line(format_entry(d));
        m_ring.push_back(std::move(d));
        if(m_ring.size() > kRing) m_ring.pop_front();
    }

    /// Most recent diagnostics, oldest first.
    [[nodiscard]]
    std::vector<Diagnostic> recent() const {
        std::lock_guard lock(m_mutex);
        return {m_ring.begin(), m_ring.end()};
    }

    /// Newest diagnostic at or above `min` that fired within `max_age`.
    [[nodiscard]]
    std::optional<Diagnostic> latest(Severity min,
                                     std::chrono::milliseconds max_age) const {
        std::lock_guard lock(m_mutex);
        const auto now = std::chrono::steady_clock::now();
        for(auto it = m_ring.rbegin(); it != m_ring.rend(); ++it)
            if(it->severity >= min && now - it->last <= max_age) return *it;
        return std::nullopt;
    }

    [[nodiscard]]
    size_t total(Severity s) const {
        std::lock_guard lock(m_mutex);
        return m_totals[static_cast<size_t>(s)];
    }

    /// Where the log goes (opens it on first call).
    [[nodiscard]]
    std::string path() {
        std::lock_guard lock(m_mutex);
        open_locked();
        return m_path;
    }

    /// Raw log fd for the crash handler (-1 if it can't be opened).
    [[nodiscard]]
    int fd() {
        std::lock_guard lock(m_mutex);
        open_locked();
        return m_fd;
    }

    /// Points stderr at the log so stray writes (std::cerr, libstdc++
    /// terminate messages, library output) can't land in the UI. The
    /// original fd is kept for the final crash/summary message.
    void redirect_stderr() {
        const char* opt = std::getenv("TRUX_STDERR");
        if(opt && std::string_view(opt) == "tty") return;
        std::lock_guard lock(m_mutex);
        open_locked();
        if(m_fd < 0 || m_saved_stderr >= 0) return;
        m_saved_stderr = ::dup(STDERR_FILENO);
        ::dup2(m_fd, STDERR_FILENO);
    }

    void restore_stderr() {
        std::lock_guard lock(m_mutex);
        if(m_saved_stderr < 0) return;
        ::dup2(m_saved_stderr, STDERR_FILENO);
        ::close(m_saved_stderr);
        m_saved_stderr = -1;
    }

    /// fd that reaches the user's real terminal output, even while
    /// stderr is redirected. Async-signal-safe read.
    [[nodiscard]]
    int user_stderr() const noexcept {
        return m_saved_stderr >= 0 ? m_saved_stderr : STDERR_FILENO;
    }

    /// Appends raw text to the log (for backtraces etc.).
    void write_raw(std::string_view s) {
        std::lock_guard lock(m_mutex);
        open_locked();
        if(m_fd >= 0) (void)!::write(m_fd, s.data(), s.size());
    }

private:
    static constexpr size_t kRing = 256;

    mutable std::mutex m_mutex;
    std::deque<Diagnostic> m_ring;
    size_t m_totals[4]{};
    std::string m_path;
    int m_fd{-1};
    int m_saved_stderr{-1};
    bool m_opened{false};

    static std::string_view short_file(const char* f) {
        std::string_view s = f;
        if(auto p = s.find("trux/"); p != std::string_view::npos)
            return s.substr(p);
        if(auto p = s.rfind('/'); p != std::string_view::npos)
            return s.substr(p + 1);
        return s;
    }

    static std::string format_entry(const Diagnostic& d) {
        auto now = std::chrono::system_clock::to_time_t(
            std::chrono::system_clock::now());
        std::tm tm{};
        localtime_r(&now, &tm);
        char ts[16];
        std::strftime(ts, sizeof ts, "%H:%M:%S", &tm);
        std::string line = std::format("{} {:<5} {}: {}  ({})",
                                       ts, label(d.severity), d.source,
                                       d.message, d.where);
        if(!d.hint.empty()) line += std::format("\n         hint: {}", d.hint);
        return line;
    }

    void open_locked() {
        if(m_opened) return;
        m_opened = true;
        const char* env = std::getenv("TRUX_LOG");
        m_path = (env && *env) ? std::string(env)
                               : std::format("/tmp/trux-{}.log", ::getpid());
        m_fd = ::open(m_path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC,
                      0644);
    }

    void write_line(const std::string& s) {
        open_locked();
        if(m_fd < 0) return;
        std::string out = s + "\n";
        (void)!::write(m_fd, out.data(), out.size());
    }
};

/// @name Reporting
/// @{
inline void report(Severity s, std::string_view source, std::string_view msg,
                   std::string_view hint = {},
                   std::source_location loc = std::source_location::current()) {
    Log::get().report(s, source, msg, hint, loc);
}
inline void info(std::string_view source, std::string_view msg,
                 std::string_view hint = {},
                 std::source_location loc = std::source_location::current()) {
    Log::get().report(Severity::Info, source, msg, hint, loc);
}
inline void warn(std::string_view source, std::string_view msg,
                 std::string_view hint = {},
                 std::source_location loc = std::source_location::current()) {
    Log::get().report(Severity::Warn, source, msg, hint, loc);
}
inline void error(std::string_view source, std::string_view msg,
                  std::string_view hint = {},
                  std::source_location loc = std::source_location::current()) {
    Log::get().report(Severity::Error, source, msg, hint, loc);
}
/// @}

// ── crash handling ────────────────────────────────────────────────────

/// Must be async-signal-safe: write(2) and tcsetattr(3) only.
using RestoreHook = void (*)() noexcept;

namespace detail {
inline std::atomic<RestoreHook>& restore_hook() {
    static std::atomic<RestoreHook> hook{nullptr};
    return hook;
}
inline std::atomic_flag& crashing() {
    static std::atomic_flag f = ATOMIC_FLAG_INIT;
    return f;
}
inline char* fatal_buf() {
    static char buf[1024];
    return buf;
}

/// Readable backtrace of the current stack, innermost first. Frames are
/// resolved to "function at file:line" through addr2line when it is
/// installed (needs -g for lines), else to demangled symbols. Frames
/// inside libc/libstdc++/diag internals at the top, and everything
/// after main(), are trimmed. Not async-signal-safe; only used on the
/// way down, after the terminal has been restored.
inline std::string backtrace_text(int max_frames = 40) {
    struct Frame {
        std::string module, text;
        uintptr_t off{};
        bool system{};
    };
    void* addrs[64];
    int n = ::backtrace(addrs, std::min(max_frames + 8, 64));
    std::vector<Frame> frames(static_cast<size_t>(n));

    for(int i = 0; i < n; i++) {
        Dl_info info{};
        auto& f = frames[static_cast<size_t>(i)];
        // return address - 1 lands inside the call instruction
        auto pc = reinterpret_cast<uintptr_t>(addrs[i]) - 1;
        if(::dladdr(reinterpret_cast<void*>(pc), &info) && info.dli_fname) {
            f.module = info.dli_fname;
            f.off = pc - reinterpret_cast<uintptr_t>(info.dli_fbase);
            f.system = f.module.starts_with("/lib") || f.module.starts_with("/usr/lib");
            if(info.dli_sname)
                f.text = demangle(info.dli_sname);
            else
                f.text = std::format("{}+{:#x}", f.module, f.off);
        } else f.text = std::format("{:#x}", pc);
    }

    // One addr2line run per module (usually just the executable).
    for(size_t i = 0; i < frames.size(); i++) {
        if(frames[i].module.empty() || frames[i].system ||
           frames[i].module.find('\'') != std::string::npos)
            continue;
        std::string cmd = "addr2line -Cfpe '" + frames[i].module + "'";
        std::vector<size_t> idx;
        for(size_t j = i; j < frames.size(); j++)
            if(frames[j].module == frames[i].module && !frames[j].system) {
                cmd += std::format(" {:#x}", frames[j].off);
                idx.push_back(j);
            }
        cmd += " 2>/dev/null";
        std::vector<std::string> lines;
        if(FILE* p = ::popen(cmd.c_str(), "r")) {
            char buf[1024];
            while(std::fgets(buf, sizeof buf, p)) {
                std::string l = buf;
                while(!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
                lines.push_back(std::move(l));
            }
            ::pclose(p);
        }
        if(lines.size() == idx.size())
            for(size_t k = 0; k < idx.size(); k++)
                if(!lines[k].starts_with("??")) {
                    if(auto d = lines[k].find(" (discriminator"); d != std::string::npos)
                        lines[k].resize(d);
                    frames[idx[k]].text = lines[k];
                }
        for(size_t j : idx) frames[j].module.clear();  // done with this module
        frames[i].module.clear();
    }

    // Trim: leading system/diag frames, trailing frames after main.
    size_t first = 0;
    while(first < frames.size() &&
          (frames[first].system || frames[first].text.find("trux::diag::") != std::string::npos ||
           frames[first].text.find("diag/diag.hpp") != std::string::npos ||
           frames[first].text.find("__cxxabiv1") != std::string::npos ||
           frames[first].text.find("__gnu_cxx") != std::string::npos ||
           frames[first].text.starts_with("std::terminate") ||
           frames[first].text.find("__restore_rt") != std::string::npos))
        first++;
    std::string out;
    int shown = 0;
    for(size_t i = first; i < frames.size() && shown < max_frames; i++, shown++) {
        out += std::format("  #{:<2} {}\n", shown, frames[i].text);
        if(frames[i].text.starts_with("main ") || frames[i].text.starts_with("main("))
            break;
    }
    return out.empty() ? "  (no backtrace available)\n" : out;
}
}  // namespace detail

/// Registers (or clears, with nullptr) the hook that puts the terminal
/// back to normal. Called first thing on any fatal path.
inline void set_restore_hook(RestoreHook hook) {
    detail::restore_hook().store(hook);
}

namespace detail {
/// The crash report, laid out like the compile-time errors:
///   error[R0001]: <first line of `what`>
///      = why:  <remaining lines>
///      = at:   <first frames>
///      = log:  <path>
inline std::string format_fatal(std::string_view what, std::string_view frames,
                                std::string_view log_path, bool color) {
    const char* red = color ? "\x1b[1;31m" : "";
    const char* bold = color ? "\x1b[1m" : "";
    const char* blue = color ? "\x1b[1;34m" : "";
    const char* cyan = color ? "\x1b[1;36m" : "";
    const char* rst = color ? "\x1b[0m" : "";
    auto lines = [](std::string_view text) {
        std::vector<std::string_view> out;
        while(!text.empty()) {
            auto nl = text.find('\n');
            auto line = text.substr(0, nl);
            while(!line.empty() && line.front() == ' ') line.remove_prefix(1);
            if(!line.empty()) out.push_back(line);
            if(nl == std::string_view::npos) break;
            text.remove_prefix(nl + 1);
        }
        return out;
    };
    auto w = lines(what);
    std::string out = std::format("\n{}error[R0001]{}{}: {} (terminal restored){}\n",
                                  red, rst, bold, w.empty() ? "fatal error" : w[0], rst);
    for(size_t i = 1; i < w.size(); i++)
        out += std::format("   {}{}{} {}\n", blue, i == 1 ? "= why: " : "       ", rst, w[i]);
    bool first = true;
    for(auto f : lines(frames)) {
        out += std::format("   {}{}{} {}\n", cyan, first ? "= at:  " : "       ", rst, f);
        first = false;
    }
    out += std::format("   {}= log: {} {}\n\n", blue, rst, log_path);
    return out;
}
}  // namespace detail

/// Terminal is restored, now say what happened and die. Never returns.
/// Full backtrace goes to the log; the user sees a short report.
[[noreturn]]
inline void fatal_exit(std::string_view what, int /*unused*/ = 0) {
    if(detail::crashing().test_and_set()) std::_Exit(134);  // re-entered
    if(auto hook = detail::restore_hook().load()) hook();
    ::alarm(3);  // if formatting below deadlocks, die anyway

    auto& log = Log::get();
    std::string bt = detail::backtrace_text();
    log.report(Severity::Fatal, "trux", what);
    log.write_raw("  backtrace:\n" + bt);

    // First few frames inline, rest in the log.
    std::string head;
    int lines = 0;
    for(size_t pos = 0; pos < bt.size() && lines < 6; lines++) {
        auto nl = bt.find('\n', pos);
        head += bt.substr(pos, nl - pos + 1);
        pos = nl + 1;
    }
    std::string msg = detail::format_fatal(what, head, log.path(),
                                           ::isatty(log.user_stderr()) != 0);
    (void)!::write(log.user_stderr(), msg.data(), msg.size());

    ::signal(SIGABRT, SIG_DFL);
    std::abort();
}

/// Assertion failure: logs expression + message, then fatal_exit().
[[noreturn]]
inline void assert_failed(const char* expr, std::string_view msg,
                          std::source_location loc = std::source_location::current()) {
    fatal_exit(std::format("assertion failed: {}\n  {}\n  at {}:{}", expr, msg,
                           loc.file_name(), loc.line()),
               3);
}

namespace detail {
inline void terminate_handler() {
    std::string what = "terminate() called with no active exception";
    if(std::current_exception()) what = "uncaught " + describe_current_exception();
    fatal_exit(what, 3);
}

extern "C" inline void signal_handler(int sig) {
    const char* name = sig == SIGSEGV   ? "SIGSEGV (invalid memory access)"
                       : sig == SIGBUS  ? "SIGBUS (bad memory access)"
                       : sig == SIGFPE  ? "SIGFPE (arithmetic error)"
                       : sig == SIGILL  ? "SIGILL (illegal instruction)"
                                        : "SIGABRT (abort)";
    fatal_exit(std::format("fatal signal {}", name), 3);
}
}  // namespace detail

/// Installs std::terminate + SIGSEGV/SIGBUS/SIGFPE/SIGILL/SIGABRT
/// handlers that restore the terminal (via `hook`) before reporting.
/// Idempotent. Terminal::init() calls this; apps rarely need to.
inline void install_crash_handlers(RestoreHook hook) {
    set_restore_hook(hook);
    static std::once_flag once;
    std::call_once(once, [] {
        std::set_terminate(detail::terminate_handler);

        // Own stack so a stack-overflow SEGV can still be handled.
        static char altstack[64 * 1024];
        stack_t ss{};
        ss.ss_sp = altstack;
        ss.ss_size = sizeof altstack;
        ::sigaltstack(&ss, nullptr);

        for(int sig : {SIGSEGV, SIGBUS, SIGFPE, SIGILL, SIGABRT}) {
            struct sigaction sa{};
            sa.sa_handler = detail::signal_handler;
            sigemptyset(&sa.sa_mask);
            sa.sa_flags = SA_ONSTACK | SA_RESETHAND;
            ::sigaction(sig, &sa, nullptr);
        }
    });
}

}  // namespace trux::diag

/// Assert with a message and values. Active in debug builds or when
/// TRUX_CHECKS is defined; compiles away otherwise (like assert()).
///   TRUX_ASSERT(contains(pos), "({}, {}) outside {}x{}", pos.x, pos.y, w, h);
#if !defined(NDEBUG) || defined(TRUX_CHECKS)
#define TRUX_ASSERT(cond, ...)                                              \
    do {                                                                    \
        if(!(cond)) [[unlikely]]                                            \
            ::trux::diag::assert_failed(#cond, std::format(__VA_ARGS__));   \
    } while(0)
#else
#define TRUX_ASSERT(cond, ...) ((void)0)
#endif

/// Warn once per call site (hot paths: builds run every frame).
#define TRUX_WARN_ONCE(source, ...)                                         \
    do {                                                                    \
        static std::atomic<bool> trux_warned_{false};                       \
        if(!trux_warned_.exchange(true))                                    \
            ::trux::diag::warn(source, std::format(__VA_ARGS__));           \
    } while(0)
