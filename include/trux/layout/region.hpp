#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/rect.hpp"

#include <cstddef>

namespace trux::layout {

struct Split;

// Composition region to be fed into the renderer
class Region {
public:
    constexpr Region(Position pos, Size size) : m_rect{pos, size} {}
    constexpr Position position() const noexcept { return m_rect.position; }
    constexpr Size     size() const noexcept { return m_rect.size; }

    [[nodiscard]]
    Split v_split(int percent) const;
    [[nodiscard]]
    Split h_split(int percent) const;

    [[nodiscard]]
    constexpr Rect rect() const noexcept {
        return m_rect;
    }

    [[nodiscard]]
    constexpr bool contains(Position pos) const noexcept {
        return pos.x >= m_rect.position.x && pos.y >= m_rect.position.y &&
               pos.x < m_rect.position.x + m_rect.size.width &&
               pos.y < m_rect.position.y + m_rect.size.height;
    }

    [[nodiscard]]
    constexpr Position absolute(Position local) const noexcept {
        return {m_rect.position.x + local.x, m_rect.position.y + local.y};
    }

private:
    Rect m_rect;
};

struct Split {
    Region first;
    Region second;

    constexpr Region& operator[](std::size_t i) noexcept {
        return i == 0 ? first : second;
    }
    constexpr const Region& operator[](std::size_t i) const noexcept {
        return i == 0 ? first : second;
    }

    constexpr Region* begin() noexcept { return &first; }
    constexpr Region* end() noexcept { return &second + 1; }

    constexpr const Region* begin() const noexcept { return &first; }
    constexpr const Region* end() const noexcept { return &second + 1; }
};

}  // namespace trux::layout
