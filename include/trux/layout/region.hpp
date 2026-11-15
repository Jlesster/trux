/// @file region.hpp
/// @brief Region: the recursive split-based layout tree that owns
///        component placement and drives rendering.

#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/rect.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <vector>

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
concept ComponentType = requires(const T& component, layout::Region area,
                                 renderer::DrawCommandBuffer& commands) {
    { component.flags } -> std::same_as<const ComponentFlags&>;
    { component.build(area, commands) } -> std::same_as<void>;
};

/// Type-erasing wrapper that lets any ComponentType be stored behind
/// a `component::ComponentBase*` and owned by a Region.
///
/// @tparam T A type satisfying ComponentType.
template <ComponentType T>
struct ComponentWrapper;

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
    /// How many children the split produces (>= 2). Part of the key so
    /// `v_split<2>(30)` and `v_split<3>(30)` are cached separately.
    int parts = 2;

    std::vector<std::optional<int>> sizes{};

    /// Compares keys field-by-field; used to deduplicate cached splits.
    constexpr auto operator<=>(const SplitKey&) const = default;
};

/// Passed in a SizeRequest to take every cell the parent split offers.
inline constexpr int kFill = std::numeric_limits<int>::max();

/// Placeholder in a sizes table: this child takes an equal share of whatever
/// the sized children leave over.
inline constexpr int kAuto = std::numeric_limits<int>::min();

/// What a component asks of the Split its Region lives in. An unset
/// dimension keeps the size the split would give it naturally. Values
/// are clamped to the parent's extent; kFill means "all of it".
struct SizeRequest {
    std::optional<int> width;
    std::optional<int> height;
    constexpr bool operator==(const SizeRequest&) const = default;
};

struct RegionNode;
struct Split;
struct Leaves;
template <std::size_t N>
struct SplitN;

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

    /// Rect of the root of the tree this region belongs to (the whole
    /// terminal for the Renderer's root). The space a component may
    /// overlay; equals rect() for a region with no parent.
    Rect root_rect() const noexcept;

    /// Splits this region vertically (side by side) into `N` children.
    /// The first child gets `percent` of the width; the remaining
    /// width is divided equally among the other `N - 1` children
    /// (leftover cells go to the earliest of them). `N` defaults to 2,
    /// which is the classic "first gets percent, second gets the rest".
    ///
    /// Calling this again with the same arguments on the same region
    /// returns the previously created split rather than creating a new
    /// one, so it is safe to call every frame.
    ///
    /// @code
    /// auto [a, b, c] = region.v_split<3>(30);  // 30% / 35% / 35%
    /// @endcode
    ///
    /// @tparam N Number of children, >= 2.
    /// @param percent Width of the first child, clamped to [0, 100].
    template <std::size_t N = 2>
    [[nodiscard]]
    SplitN<N>& v_split(int percent) const {
        return split_as<N>({Orientation::Vertical,
                            false,
                            std::clamp(percent, 0, 100),
                            false,
                            int(N)});
    }

    /// Horizontal (stacked) counterpart of v_split(); `percent` is the
    /// first child's share of the height.
    template <std::size_t N = 2>
    [[nodiscard]]
    SplitN<N>& h_split(int percent) const {
        return split_as<N>({Orientation::Horizontal,
                            false,
                            std::clamp(percent, 0, 100),
                            false,
                            int(N)});
    }

    /// v_split() with an absolute cell count for the first child's
    /// width (negative counts from the far edge). The remaining cells
    /// are divided equally among the other `N - 1` children.
    template <std::size_t N = 2>
    [[nodiscard]]
    SplitN<N>& v_split_fixed(int cells) const {
        return split_as<N>({Orientation::Vertical, true, cells, false, int(N)});
    }

    /// h_split() with an absolute cell count for the first child's height.
    template <std::size_t N = 2>
    [[nodiscard]]
    SplitN<N>& h_split_fixed(int cells) const {
        return split_as<N>(
            {Orientation::Horizontal, true, cells, false, int(N)});
    }

    /// Like v_split(), but neighbouring children overlap by one column
    /// at every seam so a border drawn by one child's edge is shared
    /// with the next rather than doubled up.
    template <std::size_t N = 2>
    [[nodiscard]]
    SplitN<N>& v_split_shared(int percent) const {
        return split_as<N>({Orientation::Vertical,
                            false,
                            std::clamp(percent, 0, 100),
                            true,
                            int(N)});
    }

    /// Like h_split(), but neighbouring children overlap by one row at
    /// every seam. See v_split_shared().
    template <std::size_t N = 2>
    [[nodiscard]]
    SplitN<N>& h_split_shared(int percent) const {
        return split_as<N>({Orientation::Horizontal,
                            false,
                            std::clamp(percent, 0, 100),
                            true,
                            int(N)});
    }

    template <std::size_t N>
    [[nodiscard]] SplitN<N>& split_by(SplitKey key) const {
        key.parts = int(N);
        return split_as<N>(key);
    }
    [[nodiscard]] Leaves leaf() const;

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
    template <typename T>
    Region& operator=(T comp) {
        set_component(
            std::make_shared<component::ComponentWrapper<T>>(std::move(comp)));
        return *this;
    }

    /// Lets the component built into this region resize it. Along the
    /// parent split's axis the sibling absorbs the difference (and
    /// shrinks to nothing on kFill); across it the region is simply
    /// narrowed. The request is stored, so it survives
    /// propagate_resize() and may be repeated every frame for free.
    /// A no-op for regions with no parent split (e.g. the root).
    void request_size(SizeRequest request) const;

    /// Renders this region's assigned component (if any) by
    /// appending draw commands to `cmd`. No-op if this region has no
    /// component assigned.
    void build(renderer::DrawCommandBuffer& cmd) const;

private:
    friend void propagate_resize(Region&, Size);
    friend Region init(Size);
    friend void resize_all_roots(Size);
    friend class Leaves;

    explicit Region(std::shared_ptr<RegionNode> node)
        : m_node(std::move(node)) {};

    void set_component(std::shared_ptr<component::ComponentBase> c);

    /// Cache lookup / creation shared by every *_split* overload.
    /// `make` allocates the concrete SplitN<N> so the cached object's
    /// dynamic type always matches `key.parts`.
    Split& split_impl(const SplitKey& key,
                      std::shared_ptr<Split> (*make)()) const;

    template <std::size_t N>
    SplitN<N>& split_as(const SplitKey& key) const {
        static_assert(N >= 2, "a split needs at least two children");
        return static_cast<SplitN<N>&>(split_impl(key, [] {
            return std::static_pointer_cast<Split>(
                std::make_shared<SplitN<N>>());
        }));
    }

    /// Recomputes both children of `split` from the parent's rect, the
    /// split's key and any SizeRequests held by the children.
    static void layout_split(RegionNode& parent, const SplitKey& key,
                             Split& split);
    /// layout_split() for every split under `node`, recursively.
    static void reflow(RegionNode& node);

    void set_name(std::string_view name) const;

    std::shared_ptr<RegionNode> m_node;
    Rect m_rect;
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

/// The child Regions produced by splitting a Region.
///
/// Split itself is count-agnostic (use indexing / range-for / size()).
/// The splitting methods return SplitN<N>, whose count is known at
/// compile time and which therefore supports structured bindings:
/// `auto [left, right] = region.v_split(50);` or
/// `auto [a, b, c] = region.v_split<3>(30);`.
struct Split {
    /// The child regions in order: left-to-right for vertical splits,
    /// top-to-bottom for horizontal splits.
    std::vector<Region> parts;
    /// Flags controlling how this split itself is drawn (e.g. a
    /// divider/border between the children).
    component::ComponentFlags flags{};

    Region& operator[](std::size_t i) noexcept { return parts[i]; }
    const Region& operator[](std::size_t i) const noexcept { return parts[i]; }
    std::size_t size() const noexcept { return parts.size(); }

    /// @name Iteration
    /// Enables `for (Region& r : split)` over every child.
    /// @{
    auto begin() noexcept { return parts.begin(); }
    auto end() noexcept { return parts.end(); }
    auto begin() const noexcept { return parts.begin(); }
    auto end() const noexcept { return parts.end(); }
    /// @}

    /// Renders every child into `cmd`. `area` is currently unused
    /// by the implementation but reserved for drawing the split's
    /// own decoration (e.g. a divider) via `flags`.
    void build(Region area, renderer::DrawCommandBuffer& cmd) const;
};

/// A Split known to have exactly N children. Adds nothing but the
/// compile-time count that structured bindings need.
template <std::size_t N>
struct SplitN : Split {};

class Leaves {
public:
    Leaves() = default;

    [[nodiscard]] Region* find(std::string_view name) const;
    [[nodiscard]] Region& operator[](std::string_view name) const;
    [[nodiscard]] bool contains(std::string_view name) const;
    [[nodiscard]] bool is_leaf(std::string_view name) const;
    [[nodiscard]] std::size_t size() const;
    [[nodiscard]] std::vector<std::string> names() const;
    void each(const std::function<void(std::string_view, Region&)>& fn) const;

    template <class T>
    Region& set(std::string_view name, T comp) const {
        Region& r = (*this)[name];
        r = std::move(comp);
        return r;
    }
    void request_size(std::string_view name, SizeRequest req) const {
        (*this)[name].request_size(req);
    }
    bool erase(std::string_view name) const;
    void clear() const;

    // legacy form: only the first child is sized, the rest share equally
    template <std::size_t N>
    SplitN<N>& vsplit(std::string_view parent, int percent,
                      const std::string_view (&names)[N]) const {
        return head(parent, Orientation::Vertical, false, percent, names);
    }
    template <std::size_t N>
    SplitN<N>& hsplit(std::string_view parent, int percent,
                      const std::string_view (&names)[N]) const {
        return head(parent, Orientation::Horizontal, false, percent, names);
    }
    template <std::size_t N>
    SplitN<N>& vsplit_fixed(std::string_view parent, int cells,
                            const std::string_view (&names)[N]) const {
        return head(parent, Orientation::Vertical, true, cells, names);
    }
    template <std::size_t N>
    SplitN<N>& hsplit_fixed(std::string_view parent, int cells,
                            const std::string_view (&names)[N]) const {
        return head(parent, Orientation::Horizontal, true, cells, names);
    }

    // sizes table + names table: leaf.vsplit("root", {20, 50}, {"a", "b", "c"})
    template <std::size_t M, std::size_t N>
    SplitN<N>& vsplit(std::string_view parent, const int (&sizes)[M],
                      const std::string_view (&names)[N]) const {
        return run(parent, Orientation::Vertical, false, sizes, names);
    }
    template <std::size_t M, std::size_t N>
    SplitN<N>& hsplit(std::string_view parent, const int (&sizes)[M],
                      const std::string_view (&names)[N]) const {
        return run(parent, Orientation::Horizontal, false, sizes, names);
    }
    template <std::size_t M, std::size_t N>
    SplitN<N>& vsplit_fixed(std::string_view parent, const int (&sizes)[M],
                            const std::string_view (&names)[N]) const {
        return run(parent, Orientation::Vertical, true, sizes, names);
    }
    template <std::size_t M, std::size_t N>
    SplitN<N>& hsplit_fixed(std::string_view parent, const int (&sizes)[M],
                            const std::string_view (&names)[N]) const {
        return run(parent, Orientation::Horizontal, true, sizes, names);
    }

private:
    friend class Region;
    explicit Leaves(Region root)
        : m_root(std::move(root)) {}

    template <std::size_t N>
    SplitN<N>& head(std::string_view parent, Orientation o, bool fixed,
                    int value, const std::string_view (&names)[N]) const {
        SplitKey key{o, fixed, fixed ? value : std::clamp(value, 0, 100)};
        return finish<N>(parent, std::move(key), names);
    }
    template <std::size_t M, std::size_t N>
    SplitN<N>& run(std::string_view parent, Orientation o, bool fixed,
                   const int (&sizes)[M],
                   const std::string_view (&names)[N]) const {
        static_assert(M <= N, "more sizes than names");
        SplitKey key{o, fixed, 0};
        for(std::size_t i = 0; i < M; ++i) {
            const int v = sizes[i];
            key.sizes.push_back(
                v == kAuto
                    ? std::nullopt
                    : std::optional<int>(fixed ? v : std::clamp(v, 0, 100)));
        }
        return finish<N>(parent, std::move(key), names);
    }
    template <std::size_t N>
    SplitN<N>& finish(std::string_view parent, SplitKey key,
                      const std::string_view (&names)[N]) const {
        auto& s = (*this)[parent].template split_by<N>(std::move(key));
        for(std::size_t i = 0; i < N; ++i) s[i].set_name(names[i]);
        return s;
    }

    Region m_root;  // handle onto the tree's root; owns the name registry
};

}  // namespace trux::layout

namespace trux::layout {

/// `std::get`-style accessors enabling structured bindings on SplitN.
///
/// The `SplitN&&` overload is needed for value-form bindings
/// (`auto [a, b] = split;`): the standard calls `get<i>` on the hidden
/// copy as an xvalue, which would otherwise pick the const overload.
template <std::size_t I, std::size_t N>
constexpr Region& get(SplitN<N>& s) noexcept {
    static_assert(I < N, "index out of range for SplitN");
    return s.parts[I];
}
/// @copydoc get(SplitN<N>&)
template <std::size_t I, std::size_t N>
constexpr const Region& get(const SplitN<N>& s) noexcept {
    static_assert(I < N, "index out of range for SplitN");
    return s.parts[I];
}
/// @copydoc get(SplitN<N>&)
template <std::size_t I, std::size_t N>
constexpr Region& get(SplitN<N>&& s) noexcept {
    static_assert(I < N, "index out of range for SplitN");
    return s.parts[I];
}

}  // namespace trux::layout

/// @cond STRUCTURED_BINDING_TRAITS
/// `tuple_size`/`tuple_element` specializations required for
/// `trux::layout::Split` to support structured bindings.
namespace std {
template <std::size_t N>
struct tuple_size<trux::layout::SplitN<N>>
    : std::integral_constant<std::size_t, N> {};
template <std::size_t I, std::size_t N>
struct tuple_element<I, trux::layout::SplitN<N>> {
    using type = trux::layout::Region;
};
}  // namespace std
/// @endcond
