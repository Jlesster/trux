#pragma once

#include <compare>

namespace trux::layout {

/// Dimensions of a rectangular area, in terminal cells.
struct Size {
    /// Width in columns.
    int width{};
    /// Height in rows.
    int height{};

    /// Compares sizes by (width, height).
    constexpr auto operator<=>(const Size&) const = default;
};
}  // namespace trux::layout
