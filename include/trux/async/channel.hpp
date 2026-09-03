/// @file channel.hpp
/// @brief Channel: a thread-safe queue signaled by an eventfd, for
///        streaming multiple values from a background thread into
///        the main event loop (contrast async::spawn(), which
///        delivers a single one-shot result).

#pragma once
#include <sys/eventfd.h>
#include <unistd.h>

#include <mutex>
#include <queue>
#include <utility>

namespace trux::async {

/// A multi-producer queue of `T` values, readable in bulk from a
/// consumer thread that polls fd() alongside terminal input (see
/// Input::poll(Terminal&, ...)). Move-only per instance; not copyable.
template <typename T> class Channel {
public:
    /// Constructs an empty channel with its own eventfd.
    Channel() : m_fd(eventfd(0, EFD_NONBLOCK)) {}
    /// Closes the underlying eventfd.
    ~Channel() {
        if(m_fd >= 0) ::close(m_fd);
    }

    /// Enqueues `item` and signals fd() so a consumer waiting on it
    /// wakes up. Safe to call from any thread.
    void push(T item) {
        {
            std::lock_guard lock(m_mutex);
            m_queue.push(std::move(item));
        }
        uint64_t one = 1;
        (void)::write(m_fd, &one, sizeof(one));
    }

    /// Drains fd()'s pending signal and returns every item currently
    /// queued, in push order, leaving the channel empty. Call once
    /// fd() is observed readable.
    [[nodiscard]]
    std::vector<T> drain() {
        uint64_t ignore;
        (void)::read(m_fd, &ignore, sizeof(ignore));
        std::lock_guard lock(m_mutex);
        std::vector<T>  out;
        while(!m_queue.empty()) {
            out.push_back(std::move(m_queue.front()));
            m_queue.pop();
        }
        return out;
    }

    /// The eventfd to poll/select on; becomes readable whenever an
    /// item is pushed, and should be drained via drain() when it does.
    [[nodiscard]]
    int fd() const noexcept {
        return m_fd;
    }

private:
    int           m_fd;
    std::mutex    m_mutex;
    std::queue<T> m_queue;
};
}  // namespace trux::async
