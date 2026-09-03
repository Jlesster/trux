#pragma once

#include <compare>
namespace trux::layout {

/// A single cell coordinate within a terminal surface.
///
/// The origin `{0, 0}` is the top-left cell. Positions are used both
/// as local coordinates within a Region and as absolute coordinates
/// on the terminal screen (see Region::absolute()).
struct Position {
    /// Column offset from the left edge, in terminal cells.
    int x{};
    /// Row offset from the top edge, in terminal cells.
    int y{};

    /// Compares positions by (x, y). Enables `==`, `!=`, `<`, and use
    /// as an ordered map/set key.
    constexpr auto operator<=>(const Position&) const = default;
};
}  // namespace trux::layout
