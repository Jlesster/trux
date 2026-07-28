#pragma once

#include <compare>

namespace trux::layout {
struct Position {
    int x{};
    int y{};

    constexpr auto operator<=>(const Position&) const = default;
};
}  // namespace trux::layout
