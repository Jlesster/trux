#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/rect.hpp"

namespace trux::layout {
// Composition region to be fed into the renderer
class Region {
public:
    constexpr Region(Position pos, Size size) : m_rect{pos, size} {}
    constexpr Position position() const noexcept { return m_rect.position; }
    constexpr Size     size() const noexcept { return m_rect.size; }

    [[nodiscard]]
    constexpr Rect rect() const noexcept {
        return m_rect;
    }

private:
    Rect m_rect;
};
}  // namespace trux::layout
