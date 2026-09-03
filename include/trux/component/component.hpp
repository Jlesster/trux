/// @file component.hpp
/// @brief Core component machinery: the `|`-based FlagModifier syntax
///        for styling components inline, the ComponentBase/
///        ComponentWrapper type-erasure used to store heterogeneous
///        components (e.g. in Split), and the generic two-way
///        Container split.

#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/input/event.hpp"
#include "trux/layout/region.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

#include <memory>
#include <type_traits>

namespace trux::component {

/// Satisfied by any component exposing a `border_color` member that
/// can be assigned a style::Color. Renderer::push() uses this to
/// automatically recolor a component's border on focus change.
template <typename T>
concept HasBorderColor = requires(T& t, style::Color c) { t.border_color = c; };

/// A single Flag wrapped for use with the `|`/`|=` operators below,
/// letting components be styled inline, e.g. `Label{"hi"} |
/// BorderRounded | Bold`. See the predefined constants (BorderRounded,
/// Bold, etc.) below.
struct FlagModifier {
    Flag flag;

    /// Sets this modifier's flag on `target.flags`.
    template <ComponentType T> constexpr void apply(T& target) const {
        target.flags.add(flag);
    }

    /// Clears this modifier's flag on `target.flags`.
    template <ComponentType T> constexpr void clear(T& target) const {
        target.flags.remove(flag);
    }
};

/// Applies `modifier` to a temporary `component` and returns it,
/// enabling the `Label{"hi"} | Bold` construction idiom.
template <ComponentType T>
[[nodiscard]]
constexpr T operator|(T&& component, FlagModifier modifier)
    requires(!std::is_lvalue_reference_v<T>)
{
    modifier.apply(component);
    return component;
}

/// Applies `modifier` to an lvalue `component` in place and returns
/// it by reference.
template <ComponentType T>
[[nodiscard]]
constexpr T& operator|(T& component, FlagModifier modifier) {
    modifier.apply(component);
    return std::move(component);
}

/// Applies `modifier` to `component` in place.
template <ComponentType T>
constexpr T& operator|=(T& component, FlagModifier modifier) {
    modifier.apply(component);
    return component;
}

/// Combines two FlagModifiers into one carrying both flags, so chains
/// like `BorderRounded | Bold` compose before being applied to a component.
[[nodiscard]]
constexpr FlagModifier operator|(FlagModifier lhs, FlagModifier rhs) {
    return FlagModifier{lhs.flag | rhs.flag};
}

/// @name Predefined flag modifiers
/// Ready-to-use FlagModifier constants for each Flag, for the
/// `component | Modifier` styling idiom.
/// @{
inline constexpr FlagModifier BorderRounded{Flag::BorderRounded};
inline constexpr FlagModifier BorderSingle{Flag::BorderSingle};
inline constexpr FlagModifier BorderDouble{Flag::BorderDouble};
inline constexpr FlagModifier BorderBlock{Flag::BorderBlock};

inline constexpr FlagModifier Bold{Flag::Bold};
inline constexpr FlagModifier Italic{Flag::Italic};
inline constexpr FlagModifier Underline{Flag::Underline};
/// @}

/// Type-erased base class for components, used wherever heterogeneous
/// components must be stored behind a single pointer type (e.g.
/// Split's tree, Container). Prefer working with concrete component
/// types and the ComponentType concept directly when type erasure
/// isn't needed. See ComponentWrapper for the concrete-to-base bridge.
struct ComponentBase {
    virtual ~ComponentBase()                             = default;
    /// Builds this component's draw commands into `area`.
    virtual void build(layout::Region               area,
                       renderer::DrawCommandBuffer& cmd) = 0;
    /// Handles an input event; returns whether it was consumed.
    /// Default implementation never consumes (see handleable()).
    virtual bool handle(const input::Event&) { return false; }
    /// Whether this component actually wants input dispatched to it
    /// (i.e. whether the wrapped type satisfies input::Handleable).
    virtual bool handleable() const { return false; }

    /// Returns this component as a layout::Split if it is one,
    /// otherwise nullptr. Lets callers special-case splits without a
    /// full type-erased visitor.
    virtual const layout::Split* as_split() const { return nullptr; }

    /// The component's current style/border flags.
    virtual const ComponentFlags& flags() const = 0;
};

/// Type-erasing adapter that stores a concrete component `T` behind
/// the ComponentBase interface, dispatching each virtual to `T`'s
/// corresponding method (or a no-op default when `T` doesn't support
/// it, e.g. isn't input::Handleable).
template <ComponentType T> struct ComponentWrapper : ComponentBase {
    T inner;
    /// Wraps `c`, taking ownership by move.
    explicit ComponentWrapper(T c) : inner(std::move(c)) {}
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) override {
        inner.build(area, cmd);
    }
    bool handle(const input::Event& e) override {
        if constexpr(input::Handleable<T>) return inner.handle(e);
        else return false;
    }
    bool handleable() const override {
        if constexpr(input::Handleable<T>) return true;
        else return false;
    }
    const layout::Split* as_split() const override {
        if constexpr(std::is_same_v<T, layout::Split>) return &inner;
        else return nullptr;
    }
    const ComponentFlags& flags() const override { return inner.flags; }
};

/// A simple, type-erased two-way split of two owned ComponentBase
/// children. Unlike layout::Split, this is a fixed one-shot layout
/// (no runtime re-splitting/closing) and holds its children via
/// unique_ptr, so it can combine any two already-erased components.
struct Container {
    /// Percentage of the area given to `first` (0-100).
    int            percent    = 50;
    /// If true, splits left/right; if false, splits top/bottom.
    bool           horizontal = true;
    ComponentFlags flags{};

    std::unique_ptr<ComponentBase> first;
    std::unique_ptr<ComponentBase> second;

    /// Splits `area` per percent/horizontal and builds `first` and
    /// `second` into their respective halves.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto split = horizontal ? area.h_split(percent) : area.v_split(percent);
        first->build(split[0], cmd);
        second->build(split[1], cmd);
    }
};

}  // namespace trux::component
