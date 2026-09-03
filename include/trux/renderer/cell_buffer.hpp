/// @file cell_buffer.hpp
/// @brief CellBuffer: a 2D grid of Cells backing a frame, plus
///        frame-to-frame diffing.

#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell.hpp"

#include <vector>

namespace trux::renderer {

/// One changed cell, as produced by CellBuffer::diff(): the
/// position of the change and its new content.
struct CellDiff {
    layout::Position position;
    Cell             cell;
};

/// A flat, row-major grid of Cells representing one full terminal
/// frame.
///
/// Renderer keeps two of these (front and back buffer) and diffs
/// them each frame via diff() to compute the minimal set of cells
/// that actually need to be repainted to the terminal.
class CellBuffer {
public:
    /// Accesses the cell at `pos`. `pos` must satisfy contains();
    /// out-of-bounds access is asserted against in debug builds.
    Cell& at(layout::Position);
    /// @copydoc at(layout::Position)
    const Cell& at(layout::Position) const;

    /// Whether `pos` is within the buffer's current bounds.
    [[nodiscard]]
    bool contains(layout::Position) const noexcept;

    /// Current dimensions of the buffer, as set by the last resize().
    [[nodiscard]]
    layout::Size size() const noexcept;

    /// Computes the list of cells that differ between this buffer
    /// and `other`, in row-major scan order. Cells holding
    /// `kContinuationGlyph` are skipped, since they carry no
    /// independent content beyond the wide glyph that precedes them.
    ///
    /// @param other Buffer to compare against — typically the
    ///              previously committed frame.
    [[nodiscard]]
    std::vector<CellDiff> diff(const CellBuffer& other) const;

    /// Resizes the buffer to `size`, resetting every cell to a
    /// default-constructed Cell. Any previous content is discarded.
    void resize(layout::Size);

    /// Marks every cell's glyph as `kInvalidGlyph`, forcing the next
    /// diff() against this buffer to report all cells as changed.
    /// Used after a terminal resize to force a full repaint.
    void invalidate();

    /// Resets every cell to a default-constructed Cell without
    /// changing the buffer's size.
    void clear();

    /// Compares two buffers cell-by-cell (and by size).
    constexpr auto operator<=>(const CellBuffer&) const = default;

private:
    /// Converts a 2D position into the flat index into m_cells.
    [[nodiscard]]
    std::size_t index(layout::Position pos) const noexcept;

    std::vector<Cell> m_cells;
    layout::Size      m_size;
};
}  // namespace trux::renderer
