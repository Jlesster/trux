/// @file item_view.hpp
/// @brief ItemView: a rebindable, type-erased view of an item range and
///        its projection, shared by Menu, Checkbox, Dropdown and List.

#pragma once

#include "trux/diag/compile.hpp"
#include "trux/layout/position.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <concepts>
#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace trux::component {

/// Sparse per-item foreground overrides, indexed like the bound items.
/// Shared by Menu, Checkbox, Dropdown and List. Indices with no entry
/// (or an entry holding std::nullopt) have no override.
struct ItemColors {
    /// Sets the override for `index`, growing storage (no override for
    /// the gap) as needed. Pass std::nullopt to clear that item's color.
    void set(size_t index, std::optional<style::Color> color) {
        if(index >= colors_.size()) colors_.resize(index + 1);
        colors_[index] = color;
    }

    /// The override for `index`, or std::nullopt if none was set.
    std::optional<style::Color> get(size_t index) const {
        return index < colors_.size() ? colors_[index] : std::nullopt;
    }

    /// Removes every override.
    void clear() { colors_.clear(); }

private:
    std::vector<std::optional<style::Color>> colors_;
};

namespace detail {
/// Something ItemView can index: size() and operator[](size_t).
template <typename R>
concept IndexableRange = requires(R& r, std::size_t i) {
    { r.size() } -> std::convertible_to<std::size_t>;
    r[i];
};
/// A projection that turns one element of `R` into text.
template <typename R, typename Proj>
concept ProjectsToText = requires(R& r, Proj p, std::size_t i) {
    { std::invoke(p, r[i]) } -> std::convertible_to<std::string_view>;
};
}  // namespace detail

/// Erases a range `R` (anything with size() and operator[]) and a
/// projection `Proj` down to three closures, so the widgets that hold
/// an ItemView don't need to be templates. The range is captured by
/// pointer: it is NOT owned and must outlive every widget bound to it.
/// A default-constructed ItemView is unbound: size() is 0, draw() does
/// nothing and text() is empty.
struct ItemView {
    /// (Re)binds to `items`. `proj` maps an item to something
    /// convertible to std::string_view (std::identity by default).
    template <typename R, typename Proj = std::identity>
    void bind(R& items, Proj proj = {}) {
        namespace chk = diag::check;
        TRUX_CHECK(
            "T0001",
            "cannot bind this object to a list widget",
            "it has no size()" TRUX_NL
            "widgets need a container such as std::vector or std::array",
            "if it wraps a container, bind that member instead, e.g." TRUX_NL
            "`Menu(state.projects.items, ...)` not `Menu(state.projects, ...)`",
            chk::has_size<R>);
        if constexpr(chk::has_size<R>) {
            TRUX_CHECK("T0002",
                       "cannot bind this container to a list widget",
                       "it has no operator[](size_t), so items can't be read "
                       "by position" TRUX_NL
                       "(std::list, std::set and std::map can't be indexed)",
                       "copy the items into a std::vector, or bind a vector of "
                       "pointers",
                       chk::has_index<R>);
            if constexpr(chk::has_index<R>) {
                TRUX_CHECK(
                    "T0003",
                    "the item projection doesn't produce text",
                    "it must take one element and return something "
                    "convertible" TRUX_NL
                    "to std::string_view (a std::string member, for example)",
                    "pass `&Item::name`, or a lambda returning "
                    "std::string_view;" TRUX_NL
                    "omit it entirely if the elements are already strings",
                    chk::projects_to_text<R, Proj>);
                if constexpr(chk::projects_to_text<R, Proj>) {
                    auto* range = &items;
                    count_ = [range] {
                        return static_cast<size_t>(range->size());
                    };
                    draw_ = [range, proj](size_t i,
                                          layout::Position pos,
                                          style::Style style,
                                          std::optional<style::Color>
                                              fg,
                                          renderer::DrawCommandBuffer& cmd) {
                        renderer::DrawText text{
                            .position = pos,
                            .text = std::invoke(proj, (*range)[i]),
                            .style = style,
                        };
                        if(fg) text.fg = *fg;
                        cmd.push(text);
                    };
                    text_ = [range, proj](size_t i) {
                        return std::string(std::invoke(proj, (*range)[i]));
                    };
                }
            }
        }
    }

    /// Detaches from the current range; the view becomes empty.
    void unbind() {
        count_ = nullptr;
        draw_ = nullptr;
        text_ = nullptr;
    }

    bool bound() const { return static_cast<bool>(draw_); }
    size_t size() const { return count_ ? count_() : 0; }
    bool empty() const { return size() == 0; }

    /// Pushes a DrawText for item `i` at `pos`. No-op while unbound or
    /// when `i` is out of range. `fg` overrides the foreground color;
    /// std::nullopt keeps DrawText's default.
    void draw(size_t i, layout::Position pos, style::Style style,
              renderer::DrawCommandBuffer& cmd,
              std::optional<style::Color> fg = std::nullopt) const {
        if(draw_ && i < size()) draw_(i, pos, style, fg, cmd);
    }

    /// Projected text of item `i`, as an owned string. Empty while
    /// unbound or when `i` is out of range.
    std::string text(size_t i) const {
        return text_ && i < size() ? text_(i) : std::string{};
    }

private:
    std::function<size_t()> count_;
    std::function<void(size_t, layout::Position, style::Style,
                       std::optional<style::Color>,
                       renderer::DrawCommandBuffer&)>
        draw_;
    std::function<std::string(size_t)> text_;
};

}  // namespace trux::component
