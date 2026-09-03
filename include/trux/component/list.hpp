/// @file list.hpp
/// @brief List: a scrollable, non-interactive display of items
///        projected to text, with optional per-item styling.

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <functional>
#include <vector>

namespace trux::component {

/// Displays a vertically-scrollable list of `items`, one per line,
/// each rendered via `proj` (defaulting to identity, i.e. `items`
/// must already hold displayable text). Purely visual: unlike Menu,
/// List has no selection cursor and is not input::Handleable —
/// `scroll_offset` must be driven externally.
///
/// @tparam R    Range of items to display.
/// @tparam Proj Projection from an item to something DrawText accepts
///              (e.g. std::string_view). Defaults to std::identity.
template <typename R, typename Proj = std::identity> struct List {
    /// Binds the list to external `items` and `scroll_offset` storage
    /// (kept by reference, so the caller owns their lifetime).
    List(R& items, int& scroll_offset, Proj proj = {})
        : items(items), scroll_offset(scroll_offset), proj(proj) {}

    R&                        items;
    Proj                      proj;
    /// Per-item text style overrides, indexed like `items`; see
    /// set_style()/style_for(). Left shorter than `items` for indices
    /// with no override.
    std::vector<style::Style> item_styles;

    ComponentFlags flags{};
    style::Color   border_color{255, 255, 255, 255};
    /// Index of the first visible item; clamped during build() to
    /// keep the view within `items`.
    int&           scroll_offset;

    /// Sets the display style for the item at `index`, growing
    /// item_styles (with style::Style::None for the gap) as needed.
    void set_style(size_t index, style::Style style) {
        if(index >= item_styles.size())
            item_styles.resize(items.size(), style::Style::None);
        item_styles[index] = style;
    }

    /// The style to use for the item at `index`: its override if one
    /// was set, otherwise style::Style::None.
    style::Style style_for(size_t index) const {
        return index < item_styles.size() ? item_styles[index]
                                          : style::Style::None;
    }

    /// Clamps `scroll_offset` to the valid range for the current area
    /// height and draws the visible slice of `items`, one per row.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;

        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h] = content.size();

        int max_offset = std::max(0, static_cast<int>(items.size()) - h);
        scroll_offset  = std::clamp(scroll_offset, 0, max_offset);

        auto   pos = content.position();
        size_t begin =
            static_cast<size_t>(std::clamp(scroll_offset, 0, std::max(h, 0)));
        size_t end =
            std::min(items.size(), begin + static_cast<size_t>(std::max(h, 0)));
        for(size_t i = begin; i < end; i++) {
            cmd.push(renderer::DrawText{.position = pos,
                                        .text     = std::invoke(proj, items[i]),
                                        .style    = style_for(i)});
            pos.y++;
        }
    }
};

}  // namespace trux::component
