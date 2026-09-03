/// @file region.hpp
/// @brief Region: the recursive split-based layout tree that owns
///        component placement and drives rendering.

#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/rect.hpp"

#include <algorithm>
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

/// Structural constraint for any type usable as a Region's component.
///
/// A conforming type must expose a `flags` member convertible to
/// `const ComponentFlags&` and a `build(area, commands)` member
/// function that renders itself into a Region given a
/// DrawCommandBuffer to write into. Satisfying this concept is what
/// makes a type assignable to a Region via `operator=`.
///
/// @see Region::operator=
/// @see ComponentWrapper
template <typename T>
concept ComponentType = requires(const T&                     component,
                                 layout::Region               area,
                                 renderer::DrawCommandBuffer& commands) {
    { component.flags } -> std::same_as<const ComponentFlags&>;
    { component.build(area, commands) } -> std::same_as<void>;
};

/// Type-erasing wrapper that lets any ComponentType be stored behind
/// a `component::ComponentBase*` and owned by a Region.
///
/// @tparam T A type satisfying ComponentType.
template <ComponentType T> struct ComponentWrapper;

}  // namespace trux::component

namespace trux::layout {

/// Axis a Region is divided along by `v_split`/`h_split` variants.
enum class Orientation { Horizontal, Vertical };

/// General-purpose axis enumerator, independent of split direction.
enum class Axis { Horizontal, Vertical };

/// Cache key identifying one split of a Region.
///
/// Region memoizes its splits so calling e.g. `v_split(30)` twice on
/// the same Region returns the same Split rather than creating a
/// duplicate — SplitKey is what makes that lookup possible.
struct SplitKey {
    /// Whether the split divides the region vertically or horizontally.
    Orientation orientation;
    /// True if `value` is an absolute cell count (`*_split_fixed`),
    /// false if it's a percentage (`v_split`/`h_split`).
    bool fixed;
    /// The percentage (0-100) or fixed cell count the split was
    /// created with.
    int value;
    /// True if the split's two children share one row/column of
    /// overlap at the seam (see `*_split_shared`), for drawing a
    /// shared border between panes. Defaults to false.
    bool shared = false;

    /// Compares keys field-by-field; used to deduplicate cached splits.
    constexpr auto operator<=>(const SplitKey&) const = default;
};

struct RegionNode;
struct Split;

/// A rectangular area of the terminal that can either host a single
/// component or be recursively subdivided into a Split of two child
/// Regions.
///
/// Region is a lightweight, reference-counted handle (backed by a
/// `shared_ptr<RegionNode>`): copies of a Region refer to the same
/// underlying node, so splitting or assigning a component through
/// one copy is visible through all others. This is what lets you
/// build a layout tree once (typically starting from
/// `layout::init()`) and keep splitting it across frames without the
/// splits being recreated or losing their assigned components — see
/// `v_split`/`h_split` for the memoization behavior that relies on
/// this.
///
/// A Region either holds a component (assigned via `operator=`) or
/// has been split (via one of the `*_split*` methods) into two child
/// Regions; it is not expected to be both at once.
class Region {
public:
    /// Constructs a "detached" region: zero-size, no component, no
    /// parent, no cached splits.
    ///
    /// This exists so Region (and, transitively, Split) can be used
    /// as a struct member without needing to be initialized in a
    /// member-initializer list — declare it, then assign a real
    /// Region into it later (typically the result of `layout::init()`
    /// or a `*_split*` call) once you have something to build it
    /// from, e.g. in a constructor body:
    ///
    /// @code
    /// struct UiState {
    ///     trux::layout::Region root;
    ///     trux::layout::Split  list_panel;
    ///     trux::layout::Split  build_panel;
    ///
    ///     explicit UiState(trux::layout::Size size) {
    ///         root              = trux::layout::init(size);
    ///         auto& rows        = root.h_split(15);
    ///         list_panel        = rows[0].v_split(35);
    ///         build_panel       = rows[1].v_split(65);
    ///     }
    /// };
    /// @endcode
    ///
    /// A detached Region is safe but inert: `build()` on it is a
    /// no-op, and splitting it produces further detached (zero-size)
    /// splits rather than crashing — but it holds no real layout
    /// until assigned.
    Region();

    /// Constructs a region with the given absolute position and size.
    ///
    /// Most code should not call this directly; prefer
    /// `layout::init()` for the root region and the `*_split*`
    /// methods for children.
    Region(Position pos, Size size);

    /// Absolute top-left position of this region on the terminal.
    Position position() const noexcept;

    /// Current width/height of this region, in cells.
    Size size() const noexcept;

    /// Position and size combined.
    Rect rect() const noexcept;

    /// Splits this region vertically (side by side) so the first
    /// child gets `percent` of the width and the second gets the
    /// remainder.
    ///
    /// Calling this again with the same `percent` on the same
    /// region returns the previously created Split rather than
    /// creating a new one, so it is safe to call every frame.
    ///
    /// @param percent Width of the first child, clamped to [0, 100].
    /// @return Reference to the (possibly cached) child Split.
    [[nodiscard]]
    Split& v_split(int percent) const;

    /// Splits this region horizontally (stacked) so the first child
    /// gets `percent` of the height and the second gets the
    /// remainder. See v_split() for caching behavior.
    ///
    /// @param percent Height of the first child, clamped to [0, 100].
    [[nodiscard]]
    Split& h_split(int percent) const;

    /// Splits this region vertically using an absolute cell count
    /// for the first child's width instead of a percentage. See
    /// v_split() for caching behavior.
    ///
    /// @param cells Width of the first child in cells, clamped to
    ///              [0, this region's width].
    [[nodiscard]]
    Split& v_split_fixed(int cells) const;

    /// Splits this region horizontally using an absolute cell count
    /// for the first child's height instead of a percentage. See
    /// v_split() for caching behavior.
    ///
    /// @param cells Height of the first child in cells, clamped to
    ///              [0, this region's height].
    [[nodiscard]]
    Split& h_split_fixed(int cells) const;

    /// Like v_split(), but the two children overlap by one column at
    /// the seam, so a border/divider drawn by one child's edge is
    /// visually shared with the other rather than doubled up.
    ///
    /// @param percent Width of the first child, clamped to [0, 100].
    [[nodiscard]]
    Split& v_split_shared(int percent) const;

    /// Like h_split(), but the two children overlap by one row at
    /// the seam. See v_split_shared().
    ///
    /// @param percent Height of the first child, clamped to [0, 100].
    [[nodiscard]]
    Split& h_split_shared(int percent) const;

    /// Whether `pos` (in absolute terminal coordinates) falls within
    /// this region's bounds.
    [[nodiscard]]
    bool contains(Position pos) const noexcept;

    /// Converts a position local to this region's top-left corner
    /// into an absolute terminal position.
    [[nodiscard]]
    Position absolute(Position local) const noexcept;

    /// Raw pointer to the component currently assigned to this
    /// region, or `nullptr` if none has been assigned (e.g. this
    /// region has been split instead).
    [[nodiscard]]
    component::ComponentBase* component_ptr() const noexcept;

    /// Assigns a component to this region, replacing any previously
    /// assigned component.
    ///
    /// @tparam T Any type satisfying component::ComponentType.
    template <typename T> Region& operator=(T comp) {
        set_component(
            std::make_shared<component::ComponentWrapper<T>>(std::move(comp)));
        return *this;
    }

    /// Renders this region's assigned component (if any) by
    /// appending draw commands to `cmd`. No-op if this region has no
    /// component assigned.
    void build(renderer::DrawCommandBuffer& cmd) const;

private:
    friend void   propagate_resize(Region&, Size);
    friend Region init(Size);
    friend void   resize_all_roots(Size);

    explicit Region(std::shared_ptr<RegionNode> node)
        : m_node(std::move(node)) {};

    void set_component(std::shared_ptr<component::ComponentBase> c);

    std::shared_ptr<RegionNode> m_node;
    Rect                        m_rect;
};

/// Resizes `region`'s tree in place to fit `new_size`, recomputing
/// the rect of every descendant split according to its stored
/// SplitKey (percentage splits scale, fixed splits keep their cell
/// count). Call this when the terminal is resized.
void propagate_resize(Region&, Size);

/// Resizes every Region tree created via init() to `new_size`
/// (see resize_all_roots()). Prefer this over the Region& overload
/// when there's a single Renderer for the process.
void propagate_resize(Size new_size);

/// The pair of child Regions produced by splitting a Region.
///
/// Supports structured bindings (`auto [left, right] = region.v_split(50);`)
/// via the `get<I>`/`tuple_size`/`tuple_element` specializations below,
/// as well as indexing and range-based iteration.
struct Split {
    /// The first child: left half for vertical splits, top half for
    /// horizontal splits.
    Region first;
    /// The second child: right half for vertical splits, bottom half
    /// for horizontal splits.
    Region second;
    /// Flags controlling how this split itself is drawn (e.g. a
    /// divider/border between the two children).
    component::ComponentFlags flags{};

    /// Indexes the two children: 0 is `first`, anything else is `second`.
    constexpr Region& operator[](std::size_t i) noexcept {
        return i == 0 ? first : second;
    }
    /// @copydoc operator[]
    constexpr const Region& operator[](std::size_t i) const noexcept {
        return i == 0 ? first : second;
    }

    /// @name Iteration
    /// Enables `for (Region& r : split)` over `{first, second}`.
    /// @{
    constexpr Region* begin() noexcept { return &first; }
    constexpr Region* end() noexcept { return &second + 1; }

    constexpr const Region* begin() const noexcept { return &first; }
    constexpr const Region* end() const noexcept { return &second + 1; }
    /// @}

    /// Renders both children into `cmd`. `area` is currently unused
    /// by the implementation but reserved for drawing the split's
    /// own decoration (e.g. a divider) via `flags`.
    void build(Region area, renderer::DrawCommandBuffer& cmd) const;
};

}  // namespace trux::layout

namespace trux::layout {

/// `std::get`-style accessor enabling structured bindings on Split.
///
/// @tparam I Index; must be 0 (`first`) or 1 (`second`).
template <std::size_t I> constexpr Region& get(Split& s) noexcept {
    static_assert(I < 2,
                  "Split only exposes first/second to structured binding");
    return I == 0 ? s.first : s.second;
}
/// @copydoc get(Split&)
template <std::size_t I> constexpr const Region& get(const Split& s) noexcept {
    static_assert(I < 2,
                  "Split only exposes first/second to structured binding");
    return I == 0 ? s.first : s.second;
}
/// @copydoc get(Split&)
///
/// Needed for value-form structured bindings (`auto [a, b] = split;`,
/// as opposed to `auto& [a, b] = split;`): the standard calls
/// `get<i>` on the invented hidden copy as an xvalue, not an lvalue,
/// so without this overload the const overload above wins instead
/// (being the only one an xvalue can bind to) and the bindings come
/// out const.
template <std::size_t I> constexpr Region& get(Split&& s) noexcept {
    static_assert(I < 2,
                  "Split only exposes first/second to structured binding");
    return I == 0 ? s.first : s.second;
}

}  // namespace trux::layout

/// @cond STRUCTURED_BINDING_TRAITS
/// `tuple_size`/`tuple_element` specializations required for
/// `trux::layout::Split` to support structured bindings.
namespace std {
template <>
struct tuple_size<trux::layout::Split>
    : std::integral_constant<std::size_t, 2> {};
template <std::size_t I> struct tuple_element<I, trux::layout::Split> {
    using type = trux::layout::Region;
};
}  // namespace std
/// @endcond
