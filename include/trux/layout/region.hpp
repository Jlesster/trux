#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/rect.hpp"

#include <cstddef>
#include <memory>

namespace trux::component {
struct ComponentBase;
}

namespace trux::layout {

enum class Orientation { Horizontal, Vertical };
enum class Axis { Horizontal, Vertical };

struct SplitKey {
    Orientation orientation;
    bool        fixed;
    int         value;
    bool        shared = false;

    constexpr auto operator<=>(const SplitKey&) const = default;
};

struct RegionNode;
struct Split;

// Composition region to be fed into the renderer
class Region {
public:
    Region(Position pos, Size size);
    Position position() const noexcept;

    Size size() const noexcept;
    Rect rect() const noexcept;

    [[nodiscard]]
    Split& v_split(int percent) const;
    [[nodiscard]]
    Split& h_split(int percent) const;
    [[nodiscard]]
    Split& v_split_fixed(int cells) const;
    [[nodiscard]]
    Split& h_split_fixed(int cells) const;

    [[nodiscard]]
    Split& v_split_shared(int percent) const;
    [[nodiscard]]
    Split& h_split_shared(int percent) const;

    [[nodiscard]]
    bool contains(Position pos) const noexcept;
    [[nodiscard]]
    Position absolute(Position local) const noexcept;

private:
    friend void propagate_resize(Region&, Size);

    std::shared_ptr<RegionNode> m_node;
    Rect                        m_rect;
};
void propagate_resize(Region&, Size);

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
