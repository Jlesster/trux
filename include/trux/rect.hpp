#pragma once

#include "trux/position.hpp"
#include "trux/size.hpp"

namespace trux {
struct Rect {
    Position position{};
    Size     size{};
};
}  // namespace trux
