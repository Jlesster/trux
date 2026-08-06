#pragma once
#include <sys/eventfd.h>
#include <unistd.h>

#include <mutex>
#include <queue>
#include <utility>

namespace trux::async {

template <typename T> class Channel {
public:
    Channel() : m_fd(eventfd(0, EFD_NONBLOCK)) {}
    ~Channel() {
        if(m_fd >= 0) ::close(m_fd);
    }

    void push(T item) {
        {
            std::lock_guard lock(m_mutex);
            m_queue.push(std::move(item));
        }
        uint64_t one = 1;
        (void)::write(m_fd, &one, sizeof(one));
    }

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
