/// @file executor.hpp
/// @brief Executor: the singleton that lets background threads
///        (spawn()) safely deliver results back onto the main/UI
///        thread via an eventfd the event loop can poll alongside
///        terminal input (see Input::poll(Terminal&, ...)).

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

/// Process-wide singleton that queues callbacks posted from any
/// thread and signals an eventfd so the main event loop can wake up
/// and run them on its own thread. Also owns the worker threads
/// created by spawn_raw()/spawn().
class Executor {
public:
    /// The single shared Executor instance.
    static Executor& instance() {
        static Executor e;
        return e;
    }

    /// Queues `callback` to run on whichever thread later calls
    /// run_pending() with this executor's fd(), and signals fd() so
    /// that thread's poll wakes up. Safe to call from any thread.
    void post(std::function<void()> callback) {
        {
            std::lock_guard lock(m_mutex);
            m_callbacks.push_back(std::move(callback));
        }
        uint64_t one = 1;
        (void)::write(m_fd, &one, sizeof(one));
    }

    /// Call from the main/UI thread when fd() becomes readable:
    /// drains the eventfd and runs every callback queued by post()
    /// since the last call, on the calling thread.
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

    /// The eventfd to poll/select alongside terminal input; becomes
    /// readable whenever a callback is posted, and should be drained
    /// via run_pending() when it does.
    [[nodiscard]]
    int fd() const noexcept {
        return m_fd;
    }

    /// Runs `work` on a newly-spawned std::jthread owned by this
    /// Executor. Prefer async::spawn(), which also delivers `work`'s
    /// result back to the main thread; use this directly only when no
    /// result needs posting back.
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

/// Runs `work` on a background thread, then delivers its result to
/// `on_done` back on the thread that later calls
/// Executor::run_pending() (typically the main/UI thread). `work`
/// takes no arguments and returns a value; `on_done` is called with
/// that value once `work` completes.
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
