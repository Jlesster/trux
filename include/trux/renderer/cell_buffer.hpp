#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell.hpp"

#include <vector>

namespace trux::renderer {

struct CellDiff {
    layout::Position position;
    Cell             cell;
};

class CellBuffer {
public:
    Cell&       at(layout::Position);
    const Cell& at(layout::Position) const;

    [[nodiscard]]
    bool contains(layout::Position) const noexcept;
    [[nodiscard]]
    layout::Size size() const noexcept;
    [[nodiscard]]
    std::vector<CellDiff> diff(const CellBuffer& other) const;

    void resize(layout::Size);
    void clear();

    constexpr auto operator<=>(const CellBuffer&) const = default;

private:
    [[nodiscard]]
    std::size_t index(layout::Position pos) const noexcept;

    std::vector<Cell> m_cells;
    layout::Size      m_size;
};
}  // namespace trux::renderer
