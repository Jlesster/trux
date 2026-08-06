#include "trux/renderer/cell.hpp"

#include <cassert>
#include <trux/renderer/cell_buffer.hpp>

using namespace trux;

std::size_t renderer::CellBuffer::index(layout::Position pos) const noexcept {
    return static_cast<std::size_t>(pos.y * m_size.width + pos.x);
}

layout::Size renderer::CellBuffer::size() const noexcept { return m_size; }

bool renderer::CellBuffer::contains(layout::Position pos) const noexcept {
    return pos.x >= 0 && pos.y >= 0 && pos.x < m_size.width &&
           pos.y < m_size.height;
}

std::vector<renderer::CellDiff>
renderer::CellBuffer::diff(const CellBuffer& other) const {
    std::vector<CellDiff> changes;

    for(int y = 0; y < m_size.height; y++) {
        for(int x = 0; x < m_size.width; x++) {
            layout::Position pos{x, y};
            if(at(pos).glyph == kContinuationGlyph) continue;
            if(at(pos) != other.at({pos})) {
                changes.push_back({pos, at(pos)});
            }
        }
    }
    return changes;
}

// layout::Size renderer::CellBuffer::size() const {
//     return m_size;
// }

void renderer::CellBuffer::resize(layout::Size size) {
    m_size = size;
    m_cells.assign(static_cast<std::size_t>(size.width * size.height), Cell{});
}

void renderer::CellBuffer::invalidate() {
    for(auto& cell : m_cells) cell.glyph = kInvalidGlyph;
}

void renderer::CellBuffer::clear() { std::ranges::fill(m_cells, Cell{}); }

renderer::Cell& renderer::CellBuffer::at(layout::Position pos) {
    assert(contains(pos));
    return m_cells[index(pos)];
}

const renderer::Cell& renderer::CellBuffer::at(layout::Position pos) const {
    assert(contains(pos));
    return m_cells[index(pos)];
}
