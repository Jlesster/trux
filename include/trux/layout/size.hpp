#pragma once

#include <compare>

namespace trux::layout {
struct Size {
    int width{};
    int height{};

    constexpr auto operator<=>(const Size&) const = default;
};
}  // namespace trux::layout
