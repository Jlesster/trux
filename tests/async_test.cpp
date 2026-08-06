#include "test.hpp"

#include <poll.h>

#include <atomic>
#include <cassert>
#include <thread>
#include <trux/async/executor.hpp>
#include <trux/async/spawn.hpp>

using namespace trux;

namespace {

[[nodiscard]] bool wait_for_async(int timeout_ms = 1000) {
    pollfd pfd{async::Executor::instance().fd(), POLLIN, 0};
    return ::poll(&pfd, 1, timeout_ms) > 0;
}

}  // namespace

int main() {
    test::run("spawn delivers result to on_done", [] {
        std::atomic<bool> done{false};
        int               result = 0;

        async::spawn([] { return 42; },
                     [&](int r) {
                         result = r;
                         done   = true;
                     });

        assert(wait_for_async());
        async::Executor::instance().run_pending(
            async::Executor::instance().fd());
        assert(done.load());
        assert(result == 42);
    });

    test::run("callback does not run before drain", [] {
        std::atomic<bool> ran{false};

        async::spawn([] { return 1; }, [&](int) { ran = true; });

        assert(wait_for_async());
        assert(!ran.load());

        async::Executor::instance().run_pending(
            async::Executor::instance().fd());
        assert(ran.load());
    });

    test::run("multiple spawns all deliver", [] {
        constexpr int    count = 8;
        std::atomic<int> completed{0};

        for(int i = 0; i < count; i++) {
            async::spawn([i] { return i * i; }, [&](int) { completed++; });
        }

        int drained = 0;
        while(completed.load() < count && drained < count * 2) {
            if(wait_for_async(200)) {
                async::Executor::instance().run_pending(
                    async::Executor::instance().fd());
            }
            drained++;
        }

        assert(completed.load() == count);
    });

    test::run("work runs off the main thread", [] {
        std::atomic<bool> ran_elsewhere{false};
        auto              main_id = std::this_thread::get_id();

        async::spawn(
            [main_id, &ran_elsewhere] {
                ran_elsewhere = (std::this_thread::get_id() != main_id);
                return 0;
            },
            [](int) {});

        assert(wait_for_async());
        async::Executor::instance().run_pending(
            async::Executor::instance().fd());
        assert(ran_elsewhere.load());
    });

    return 0;
}
