/// @file layout.hpp
/// @brief Entry point for building a layout tree.

#pragma once

#include <trux/layout/region.hpp>

namespace trux::layout {

/// Creates the root Region of a layout tree, positioned at the
/// origin with the given size.
///
/// This is the usual starting point for building a UI: call
/// `init()` with the terminal's size, then assign a component or
/// split the result to build up the tree.
///
/// @param size Size of the root region, typically the terminal's
///             current size.
[[nodiscard]]
Region init(Size size);
}  // namespace trux::layout
