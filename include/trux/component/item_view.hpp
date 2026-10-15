/// @file item_view.hpp
/// @brief ItemView: a rebindable, type-erased view of an item range and
///        its projection, shared by Menu, Checkbox, Dropdown and List.

#pragma once

#include "trux/layout/position.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/style.hpp"

#include <cstddef>
#include <functional>

namespace trux::component {

/// Erases a range `R` (anything with size() and operator[]) and a
/// projection `Proj` down to two closures, so the widgets that hold an
/// ItemView don't need to be templates. The range is captured by
/// pointer: it is NOT owned and must outlive every widget bound to it.
/// A default-constructed ItemView is unbound: size() is 0 and draw()
/// does nothing.
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
                              renderer::DrawCommandBuffer& cmd) {
            cmd.push(renderer::DrawText{
                .position = pos,
                .text = std::invoke(proj, (*range)[i]),
                .style = style,
            });
        };
    }

    /// Detaches from the current range; the view becomes empty.
    void unbind() {
        count_ = nullptr;
        draw_ = nullptr;
    }

    bool bound() const { return static_cast<bool>(draw_); }
    size_t size() const { return count_ ? count_() : 0; }
    bool empty() const { return size() == 0; }

    /// Pushes a DrawText for item `i` at `pos`. No-op while unbound or
    /// when `i` is out of range.
    void draw(size_t i, layout::Position pos, style::Style style,
              renderer::DrawCommandBuffer& cmd) const {
        if(draw_ && i < size()) draw_(i, pos, style, cmd);
    }

private:
    std::function<size_t()> count_;
    std::function<void(size_t, layout::Position, style::Style,
                       renderer::DrawCommandBuffer&)>
        draw_;
};

}  // namespace trux::component
