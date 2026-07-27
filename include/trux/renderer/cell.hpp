#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/size.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <vector>

namespace trux::renderer {
struct Cell {
    char32_t glyph{U' '};

    style::Color foreground{255, 255, 255};
    style::Color background{0, 0, 0};

    style::Style style{style::Style::None};
};

class CellBuffer {
public:
    Cell&       at(layout::Position);
    const Cell& at(layout::Position) const;

    [[nodiscard]]
    bool contains(layout::Position) const noexcept;

    void resize(layout::Size);
    void clear();

private:
    std::vector<Cell> m_cells;
    layout::Size      m_size;
};

}  // namespace trux::renderer
