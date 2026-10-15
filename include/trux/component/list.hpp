/// @file list.hpp
/// @brief List: a scrollable, non-interactive display of items
///        projected to text, with optional per-item styling.

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/component/item_view.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <type_traits>
#include <utility>
#include <vector>

namespace trux::component {

/// Displays a vertically-scrollable list of items, one per line, each
/// rendered via a projection (identity by default, i.e. the items must
/// already hold displayable text). Purely visual: unlike Menu, List has
/// no selection cursor and is not input::Handleable — `offset` must be
/// driven externally.
///
/// A default-constructed List is unbound and draws only its border.
/// The item range is held by pointer and must outlive the list.
struct List {
    List() = default;

    /// Binds the list to an external `items` range. `proj` maps an item
    /// to something convertible to std::string_view.
    template <typename R, typename Proj = std::identity>
        requires(!std::same_as<std::remove_cvref_t<R>, List>)
    explicit List(R& items, Proj proj = {}) {
        bind(items, std::move(proj));
    }

    /// (Re)binds to `items`, keeping offset.
    template <typename R, typename Proj = std::identity>
    void bind(R& items, Proj proj = {}) {
        view.bind(items, std::move(proj));
    }

    /// Number of bound items; 0 while unbound.
    size_t size() const { return view.size(); }

    /// Type-erased view of the bound range and projection.
    ItemView view;

    /// Per-item text style overrides, indexed like the items; see
    /// set_style()/style_for(). Left shorter than the items for indices
    /// with no override.
    std::vector<style::Style> item_styles;

    ComponentFlags flags{};
    style::Color border_color{255, 255, 255, 255};

    /// Index of the first visible item; clamped during build() to keep
    /// the view within the items.
    mutable int offset = 0;

    /// Sets the display style for the item at `index`, growing
    /// item_styles (with style::Style::None for the gap) as needed.
    void set_style(size_t index, style::Style style) {
        if(index >= item_styles.size())
            item_styles.resize(std::max(size(), index + 1), style::Style::None);
        item_styles[index] = style;
    }

    /// The style to use for the item at `index`: its override if one
    /// was set, otherwise style::Style::None.
    style::Style style_for(size_t index) const {
        return index < item_styles.size() ? item_styles[index]
                                          : style::Style::None;
    }

    /// Clamps `offset` to the valid range for the current area height
    /// and draws the visible slice of items, one per row.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;

        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h] = content.size();

        int max_offset = std::max(0, static_cast<int>(size()) - h);
        offset = std::clamp(offset, 0, max_offset);

        auto pos = content.position();
        size_t begin = static_cast<size_t>(offset);
        size_t end =
            std::min(size(), begin + static_cast<size_t>(std::max(h, 0)));
        for(size_t i = begin; i < end; i++) {
            view.draw(i, pos, style_for(i), cmd);
            pos.y++;
        }
    }
};

}  // namespace trux::component
