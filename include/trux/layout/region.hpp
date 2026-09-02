#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/rect.hpp"

#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>

namespace trux::component {
struct ComponentBase;
}  // namespace trux::component
namespace trux::renderer {
class DrawCommandBuffer;
}

namespace trux::layout {
class Region;
}  // namespace trux::layout

namespace trux::component {

template <typename T>
concept ComponentType = requires(const T&                     component,
                                 layout::Region               area,
                                 renderer::DrawCommandBuffer& commands) {
    { component.flags } -> std::same_as<const ComponentFlags&>;
    { component.build(area, commands) } -> std::same_as<void>;
};

template <ComponentType T> struct ComponentWrapper;

}  // namespace trux::component

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

    [[nodiscard]]
    component::ComponentBase*     component_ptr() const noexcept;
    template <typename T> Region& operator=(T comp) {
        set_component(
            std::make_shared<component::ComponentWrapper<T>>(std::move(comp)));
        return *this;
    }
    void build(renderer::DrawCommandBuffer& cmd) const;

private:
    friend void propagate_resize(Region&, Size);
    void        set_component(std::shared_ptr<component::ComponentBase> c);

    std::shared_ptr<RegionNode> m_node;
    Rect                        m_rect;
};
void propagate_resize(Region&, Size);

struct Split {
    Region                    first;
    Region                    second;
    component::ComponentFlags flags{};

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

    void build(Region area, renderer::DrawCommandBuffer& cmd) const;
};

}  // namespace trux::layout

namespace trux::layout {

template <std::size_t I> constexpr Region& get(Split& s) noexcept {
    static_assert(I < 2,
                  "Split only exposes first/second to structured binding");
    return I == 0 ? s.first : s.second;
}
template <std::size_t I> constexpr const Region& get(const Split& s) noexcept {
    static_assert(I < 2,
                  "Split only exposes first/second to structured binding");
    return I == 0 ? s.first : s.second;
}

}  // namespace trux::layout

namespace std {
template <>
struct tuple_size<trux::layout::Split>
    : std::integral_constant<std::size_t, 2> {};
template <std::size_t I> struct tuple_element<I, trux::layout::Split> {
    using type = trux::layout::Region;
};
}  // namespace std
