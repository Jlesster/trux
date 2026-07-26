#pragma once

#include "style/color.hpp"
#include "style/style.hpp"

namespace trux {
struct Cell {
    char32_t glyph{U' '};

    Color foreground{0, 0, 0};
    Color background{0, 0, 0};

    Style style{Style::None};
};
}  // namespace trux
