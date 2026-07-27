#pragma once

#include <print>

namespace test {

template <typename Fn> void run(std::string_view name, Fn&& fn) {
    std::println("[ RUN    ] {}", name);
    fn();
    std::println("[     OK ] {}", name);
}
}  // namespace test
