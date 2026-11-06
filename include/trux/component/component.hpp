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

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>

/// Color enum to be passed as the index in color functions, matches ANSI
/// coloring, indexed from 0-15. 0-7 are dark while 8-15 are bright counterparts
enum class AnsiColor {
    BLACK,
    RED,
    GREEN,
    YELLOW,
    BLUE,
    MAGENTA,
    CYAN,
    WHITE,
    GRAY,
    B_RED,
    B_GREEN,
    B_YELLOW,
    B_BLUE,
    B_MAGENTA,
    B_CYAN,
    B_WHITE,
};

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
    template <ComponentType T>
    constexpr void apply(T& target) const {
        target.flags.add(flag);
    }

    /// Clears this modifier's flag on `target.flags`.
    template <ComponentType T>
    constexpr void clear(T& target) const {
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

/// A general modifier for the `component | Modifier` idiom: optional
/// style flags plus an optional foreground color. Produced by Color()
/// below and by combining a FlagModifier with one (`BorderRounded |
/// Color(2)`). Fields a component doesn't have are silently skipped.
struct Modifier {
    Flag flag{Flag::None};
    std::optional<style::Color> text_fg;
    std::optional<style::Color> selection_fg;
    std::optional<style::Color> border_fg;

    constexpr Modifier() = default;
    /// Implicit, so a FlagModifier combines with a Modifier via `|`.
    constexpr Modifier(FlagModifier m)  // NOLINT(google-explicit-constructor)
        : flag(m.flag) {}

    /// Sets the flag, and any color fields the component exposes:
    /// `text_color` and/or `selection_color` (std::optional<style::Color>).
    template <ComponentType T>
    constexpr void apply(T& target) const {
        target.flags.add(flag);
        if constexpr(requires { target.text_color = *text_fg; })
            if(text_fg) target.text_color = *text_fg;
        if constexpr(requires { target.selection_color = *selection_fg; })
            if(selection_fg) target.selection_color = *selection_fg;
        if(border_fg) {
            if constexpr(requires { target.border_color = *border_fg; })
                target.border_color = *border_fg;
            if constexpr(requires { target.border_color_fixed = *border_fg; })
                target.border_color_fixed = *border_fg;
        }
    }
};

/// Merges two modifiers: flags are OR'd, and colors set on `rhs` win.
[[nodiscard]]
constexpr Modifier operator|(Modifier lhs, Modifier rhs) {
    Modifier out = lhs;
    out.flag = lhs.flag | rhs.flag;
    if(rhs.text_fg) out.text_fg = rhs.text_fg;
    if(rhs.selection_fg) out.selection_fg = rhs.selection_fg;
    if(rhs.border_fg) out.border_fg = rhs.border_fg;
    return out;
}

template <ComponentType T>
[[nodiscard]]
constexpr T operator|(T&& component, Modifier modifier)
    requires(!std::is_lvalue_reference_v<T>)
{
    modifier.apply(component);
    return component;
}

template <ComponentType T>
[[nodiscard]]
constexpr T& operator|(T& component, Modifier modifier) {
    modifier.apply(component);
    return component;
}

template <ComponentType T>
constexpr T& operator|=(T& component, Modifier modifier) {
    modifier.apply(component);
    return component;
}

namespace detail {
/// Braced truecolor argument for Color({r, g, b}) / Color({r, g, b, a}).
/// Plain ints, so `Color({r, g, b})` with int variables doesn't narrow.
struct Rgb {
    int r{}, g{}, b{};
    int a{255};
};

constexpr uint8_t channel(int v) {
    return static_cast<uint8_t>(std::clamp(v, 0, 255));
}
}  // namespace detail

/// Sets a component's text and selection foreground to a terminal
/// palette color (0-15 follow the terminal theme, 16-255 are the
/// color cube / grayscale): `component::Color(2)`.
[[nodiscard]] constexpr Modifier Color(AnsiColor color) {
    auto c = style::Color::Ansi(detail::channel(static_cast<int>(color)));
    Modifier m;
    m.text_fg = m.selection_fg = c;
    return m;
}

/// Truecolor form: `component::Color({160, 0, 160})` or `{r, g, b, a}`.
[[nodiscard]] constexpr Modifier Color(detail::Rgb rgb) {
    style::Color c{detail::channel(rgb.r),
                   detail::channel(rgb.g),
                   detail::channel(rgb.b),
                   detail::channel(rgb.a)};
    Modifier m;
    m.text_fg = m.selection_fg = c;
    return m;
}

/// Convenience form: `component::Color(160, 0, 160)`.
[[nodiscard]] constexpr Modifier Color(int r, int g, int b, int a = 255) {
    return Color(detail::Rgb{r, g, b, a});
}

/// Border-only counterparts of Color():
/// `component::BorderColor(TermColor::RED)`,
/// `component::BorderColor({160, 0, 160})`, `BorderColor(160, 0, 160)`. Unlike
/// assigning `border_color` directly, this survives the focus recolor in
/// Renderer::push().
[[nodiscard]] constexpr Modifier BorderColor(AnsiColor color) {
    Modifier m;
    m.border_fg = style::Color::Ansi(detail::channel(static_cast<int>(color)));
    return m;
}
[[nodiscard]] constexpr Modifier BorderColor(detail::Rgb rgb) {
    Modifier m;
    m.border_fg = style::Color{detail::channel(rgb.r),
                               detail::channel(rgb.g),
                               detail::channel(rgb.b),
                               detail::channel(rgb.a)};
    return m;
}
[[nodiscard]] constexpr Modifier BorderColor(int r, int g, int b, int a = 255) {
    return BorderColor(detail::Rgb{r, g, b, a});
}

/// Type-erased base class for components, used wherever heterogeneous
/// components must be stored behind a single pointer type (e.g.
/// Split's tree, Container). Prefer working with concrete component
/// types and the ComponentType concept directly when type erasure
/// isn't needed. See ComponentWrapper for the concrete-to-base bridge.
struct ComponentBase {
    virtual ~ComponentBase() = default;
    /// Builds this component's draw commands into `area`.
    virtual void build(layout::Region area,
                       renderer::DrawCommandBuffer& cmd) = 0;
    /// Handles an input event; returns whether it was consumed.
    /// Default implementation never consumes (see handleable()).
    virtual bool handle(const input::Event&) { return false; }
    /// Whether this component actually wants input dispatched to it
    /// (i.e. whether the wrapped type satisfies input::Handleable).
    virtual bool handleable() const { return false; }
    /// Whether Tab/Shift+Tab should be offered to this component's
    /// handle() before the renderer uses them for focus traversal.
    /// Only components that define `captures_tab() const` opt in.
    virtual bool captures_tab() const { return false; }

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
template <ComponentType T>
struct ComponentWrapper : ComponentBase {
    T inner;
    /// Wraps `c`, taking ownership by move.
    explicit ComponentWrapper(T c)
        : inner(std::move(c)) {}
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
    bool captures_tab() const override {
        if constexpr(requires {
                         { inner.captures_tab() } -> std::same_as<bool>;
                     })
            return inner.captures_tab();
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
    int percent = 50;
    /// If true, splits left/right; if false, splits top/bottom.
    bool horizontal = true;
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
