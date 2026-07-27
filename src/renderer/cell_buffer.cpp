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

void renderer::CellBuffer::resize(layout::Size size) {
    m_size = size;
    m_cells.assign(static_cast<std::size_t>(size.width * size.height), Cell{});
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
