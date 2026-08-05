#pragma once

#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

namespace trux::renderer {
struct Cell {
    char32_t glyph{U' '};

    constexpr auto operator<=>(const Cell&) const = default;

    style::Color foreground{255, 255, 255};
    style::Color background{0, 0, 0};

    style::Style style{style::Style::None};
};

}  // namespace trux::renderer
