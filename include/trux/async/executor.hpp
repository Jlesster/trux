#pragma once
#include <sys/eventfd.h>
#include <unistd.h>

#include <algorithm>
#include <functional>
#include <mutex>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace trux::async {

class Executor {
public:
    static Executor& instance() {
        static Executor e;
        return e;
    }

    void post(std::function<void()> callback) {
        {
            std::lock_guard lock(m_mutex);
            m_callbacks.push_back(std::move(callback));
        }
        uint64_t one = 1;
        (void)::write(m_fd, &one, sizeof(one));
    }

    void run_pending(int fd_to_drain) {
        uint64_t ignore;
        (void)::read(fd_to_drain, &ignore, sizeof(ignore));
        std::vector<std::function<void()>> callbacks;
        {
            std::lock_guard lock(m_mutex);
            callbacks.swap(m_callbacks);
        }
        for(auto& cb : callbacks) cb();
    }

    [[nodiscard]]
    int fd() const noexcept {
        return m_fd;
    }

    template <typename WorkFn> void spawn_raw(WorkFn&& work) {
        std::lock_guard lock(m_thread_mutex);
        m_threads.emplace_back(std::forward<WorkFn>(work));
    }

private:
    Executor() : m_fd(eventfd(0, EFD_NONBLOCK)) {}

    int                                m_fd;
    std::mutex                         m_mutex;
    std::vector<std::function<void()>> m_callbacks;

    std::mutex                m_thread_mutex;
    std::vector<std::jthread> m_threads;
};

template <typename WorkFn, typename Callback>
void spawn(WorkFn&& work, Callback&& on_done) {
    using Result = std::invoke_result_t<WorkFn>;
    Executor::instance().spawn_raw(
        [work    = std::forward<WorkFn>(work),
         on_done = std::forward<Callback>(on_done)]() mutable {
            Result r = work();
            Executor::instance().post(
                [on_done = std::move(on_done), r = std::move(r)]() mutable {
                    on_done(std::move(r));
                });
        });
}
}  // namespace trux::async
