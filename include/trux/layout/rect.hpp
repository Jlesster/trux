#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/size.hpp"

namespace trux::layout {

/// An axis-aligned rectangular area: a top-left Position plus a Size.
///
/// Used throughout trux to describe the bounds of a Region or the
/// area a component is asked to draw into.
struct Rect {
    /// Top-left corner of the rectangle.
    Position position{};
    /// Width and height of the rectangle.
    layout::Size size{};

    /// Compares rects by (position, size).
    constexpr auto operator<=>(const Rect&) const = default;
};
}  // namespace trux::layout
