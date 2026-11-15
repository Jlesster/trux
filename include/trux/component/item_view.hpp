/// @file item_view.hpp
/// @brief ItemView: a rebindable, type-erased view of an item range and
///        its projection, shared by Menu, Checkbox, Dropdown and List.

#pragma once

#include "trux/layout/position.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
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
        auto* range = &items;
        count_ = [range] { return static_cast<size_t>(range->size()); };
        draw_ = [range, proj](size_t i,
                              layout::Position pos,
                              style::Style style,
                              std::optional<style::Color> fg,
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
