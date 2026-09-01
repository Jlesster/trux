#pragma once

#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <exception>
#include <print>
#include <string_view>

namespace test {

inline int g_failed = 0;
inline int g_passed = 0;

template <typename Fn> void run(std::string_view name, Fn&& fn) {
    std::println("[ RUN    ] {}", name);
    std::fflush(stdout);

    pid_t pid = fork();
    if(pid == 0) {
        try {
            fn();
        } catch(const std::exception& e) {
            std::println(
                stderr, "[  FAIL  ] {} - exception: {}", name, e.what());
            std::_Exit(1);
        } catch(...) {
            std::println(stderr, "[  FAIL  ] {} - unknown exception", name);
            std::_Exit(1);
        }
        std::_Exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);

    if(WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        std::println("[     OK ] {}", name);
        ++g_passed;
    } else if(WIFSIGNALED(status)) {
        std::println("[  FAIL  ] {} - killed by signal {} ({})",
                     name,
                     WTERMSIG(status),
                     strsignal(WTERMSIG(status)));
        ++g_failed;
    } else {
        std::println(
            "[  FAIL  ] {} - exited with status {}", name, WEXITSTATUS(status));
        ++g_failed;
    }
}

inline int summary() {
    std::println("\n{} passed, {} failed", g_passed, g_failed);
    return g_failed == 0 ? 0 : 1;
}

}  // namespace test
