#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell.hpp"

#include <vector>

namespace trux::renderer {
class CellBuffer {
public:
    Cell&       at(layout::Position);
    const Cell& at(layout::Position) const;

    [[nodiscard]]
    bool contains(layout::Position) const noexcept;
    [[nodiscard]]
    layout::Size size() const noexcept;

    void resize(layout::Size);
    void clear();

private:
    [[nodiscard]]
    std::size_t index(layout::Position pos) const noexcept;

    std::vector<Cell> m_cells;
    layout::Size      m_size;
};
}  // namespace trux::renderer
