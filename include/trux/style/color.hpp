#pragma once

#include <compare>
#include <cstdint>

namespace trux::style {
struct Color {
    uint8_t r{255};
    uint8_t g{255};
    uint8_t b{255};

    constexpr auto operator<=>(const Color&) const = default;
};
}  // namespace trux::style
