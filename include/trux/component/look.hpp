/// @file look.hpp
/// @brief Look: a plain aggregate describing a component's border, text
///        colour and title styling, accepted by the Look constructor
///        overload of Menu, Dropdown, Checkbox, TextInput, Canvas and
///        Dialog (whose DialogFields also carries a `look` member).

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component.hpp"
#include "trux/style/style.hpp"

#include <initializer_list>
#include <optional>
#include <string>
#include <variant>

namespace trux::component {

namespace detail {
inline style::Color ansi(AnsiColor c) {
    return style::Color::Ansi(channel(static_cast<int>(c)));
}
}  // namespace detail

/// Border portion of a Look. `kind` takes the same constants as the `|` idiom
/// (BorderRounded, BorderSingle, ...); leaving it unset means no border.
struct BorderLook {
    FlagModifier kind{Flag::None};
    std::optional<AnsiColor> fg;         // unfocused
    std::optional<AnsiColor> active_fg;  // focused
};

/// Wrappers that tag a value inside Look::titled({...}).
/// A bare AnsiColor / Style styles the resting title; Active{...} styles
/// the focused title; Fg{...} sets the component's text + selection colour.
struct Active {
    std::variant<AnsiColor, style::Style> v;
    explicit Active(AnsiColor c)
        : v(c) {}
    explicit Active(style::Style s)
        : v(s) {}
};
struct Fg {
    AnsiColor color;
};
using TitleOpt = std::variant<std::string, AnsiColor, style::Style, Active, Fg>;

/// Field order matters for designated init: fg, border, title,
/// selection_style.
struct Look {
    std::optional<AnsiColor> fg;  // text + selection colour
    /// Style of the selected row/option in Menu, Dropdown and Checkbox;
    /// unset keeps each component's own default.
    std::optional<style::Style> selection_style;
    BorderLook border;
    Title title;

    /// Copy of this Look with the title replaced/tweaked. Order of the
    /// arguments is irrelevant: a string is the text, a bare AnsiColor the
    /// resting title colour, a bare Style the resting title style (several
    /// OR together), Active{...} the same for the focused title, Fg{...}
    /// the component text colour. A group that is mentioned replaces this
    /// Look's value; a group that isn't keeps it.
    [[nodiscard]] Look titled(std::initializer_list<TitleOpt> opts) const {
        Look l = *this;
        std::optional<style::Style> st, ast;
        for(const auto& o : opts) {
            if(auto* txt = std::get_if<std::string>(&o)) {
                l.title.text = *txt;
            } else if(auto* c = std::get_if<AnsiColor>(&o)) {
                l.title.fg = detail::ansi(*c);
            } else if(auto* s = std::get_if<style::Style>(&o)) {
                st = st ? (*st | *s) : *s;
            } else if(auto* f = std::get_if<Fg>(&o)) {
                l.fg = f->color;
            } else {
                const auto& a = std::get<Active>(o);
                if(auto* ac = std::get_if<AnsiColor>(&a.v)) {
                    l.title.active_fg = detail::ansi(*ac);
                } else {
                    auto as = std::get<style::Style>(a.v);
                    ast = ast ? (*ast | as) : as;
                }
            }
        }
        if(st) l.title.attribs = *st;
        if(ast) l.title.active_attribs = *ast;
        return l;
    }
};

/// Applies `look` to `t`. Members a component lacks are skipped.
template <ComponentType T>
constexpr void apply_look(T& t, const Look& l) {
    if(l.border.kind.flag != Flag::None) l.border.kind.apply(t);

    if(l.fg) {
        auto c = detail::ansi(*l.fg);
        if constexpr(requires { t.text_color = c; }) t.text_color = c;
        if constexpr(requires { t.selection_color = c; }) t.selection_color = c;
    }
    // Must go through the *_fixed/_active members, or Renderer::push()
    // overwrites border_color on the next focus change.
    if(l.border.fg) {
        auto c = detail::ansi(*l.border.fg);
        if constexpr(requires { t.border_color = c; }) t.border_color = c;
        if constexpr(requires { t.border_color_fixed = c; })
            t.border_color_fixed = c;
    }
    if(l.border.active_fg) {
        if constexpr(requires {
                         t.border_color_active = detail::ansi(AnsiColor::BLACK);
                     })
            t.border_color_active = detail::ansi(*l.border.active_fg);
    }
    if constexpr(requires { t.title = l.title; })
        if(!l.title.text.empty()) t.title = l.title;
    if(l.selection_style)
        if constexpr(requires { t.selection_style = style::Style::None; })
            t.selection_style = *l.selection_style;
}

}  // namespace trux::component
