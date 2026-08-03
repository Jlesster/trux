#pragma once

#include <trux/input/key.hpp>

namespace trux::input {

struct Event {
    char32_t code{};
    bool     valid{false};

    constexpr explicit operator bool() const noexcept { return valid; }
    constexpr          operator char32_t() const noexcept { return code; }
};
}  // namespace trux::input
