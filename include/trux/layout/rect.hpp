#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/size.hpp"

namespace trux::layout {
struct Rect {
    Position     position{};
    layout::Size size{};
};
}  // namespace trux::layout
