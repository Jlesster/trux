#pragma once

#include <trux/layout/region.hpp>

namespace trux::layout {
[[nodiscard]]
constexpr Region root(Size size) noexcept {
    return Region{
        {0, 0},
        size,
    };
}
}  // namespace trux::layout
