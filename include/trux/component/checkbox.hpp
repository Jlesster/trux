/// @file checkbox.hpp
/// @brief Checkbox: a scrollable list of independently toggleable
///        items, with a movable cursor and space-to-toggle input.

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/input/event.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <functional>
#include <vector>
namespace trux::component {

/// A vertically-scrollable checklist: each item in `items` shows a
/// `[x]`/`[ ]` glyph reflecting the corresponding entry in `checked`,
/// with `cursor` indicating which row Up/Down/j/k move and Space toggles.
///
/// @tparam R    Range of items to display.
/// @tparam Proj Projection from an item to something DrawText accepts.
///              Defaults to std::identity.
template <typename R, typename Proj = std::identity> struct Checkbox {
    /// Binds the checkbox to external `items`, `checked` (must be the
    /// same length as `items`), `cursor`, and `scroll_offset` storage,
    /// all kept by reference.
    Checkbox(R&                 items,
             std::vector<bool>& checked,
             int&               cursor,
             int&               scroll_offset,
             Proj               proj = {})
        : items(items), checked(checked), cursor(cursor),
          scroll_offset(scroll_offset), proj(proj) {}

    R&                 items;
    Proj               proj;
    /// Checked state, one entry per item in `items`.
    std::vector<bool>& checked;

    /// Style applied to the item under the cursor.
    style::Style   cursor_style{style::Style::Italic};
    ComponentFlags flags{};
    style::Color   border_color{255, 255, 255, 255};

    /// Index of the item the cursor is on.
    int&        cursor;
    /// Index of the first visible item; clamped during build().
    int&        scroll_offset;
    /// Content height from the most recent build(), used by
    /// handle()'s keep_visible() to scroll the cursor into view.
    mutable int last_height{0};

    /// Clamps `scroll_offset`, then draws the visible slice of items
    /// with their checked-state glyph, highlighting the row at `cursor`.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h]    = content.size();
        last_height    = h;
        int max_offset = std::max(0, static_cast<int>(items.size()) - h);
        scroll_offset  = std::clamp(scroll_offset, 0, max_offset);

        auto   pos   = content.position();
        size_t begin = static_cast<size_t>(scroll_offset);
        size_t end =
            std::min(items.size(), begin + static_cast<size_t>(std::max(h, 0)));

        for(size_t i = begin; i < end; i++) {
            static constexpr std::string_view glyph_on  = "[x] ";
            static constexpr std::string_view glyph_off = "[ ] ";

            cmd.push(renderer::DrawText{
                .position = pos,
                .text     = checked[i] ? glyph_on : glyph_off,
            });
            cmd.push(renderer::DrawText{
                .position = {pos.x + 4, pos.y},
                .text     = std::invoke(proj, items[i]),
                .style = (static_cast<int>(i) == cursor) ? cursor_style
                                                         : style::Style::None,
            });
            pos.y++;
        }
    }

    /// Up/k and Down/j move `cursor` (wrapping) and scroll to keep it
    /// visible; Space toggles the checked state at `cursor`. Returns
    /// whether the event was consumed.
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(items.empty()) return false;
        auto keep_visible = [this] {
            if(cursor < scroll_offset) {
                scroll_offset = cursor;
            } else if(last_height > 0 &&
                      cursor >= scroll_offset + last_height) {
                scroll_offset = cursor - last_height + 1;
            }
        };
        switch(event.code) {
            case input::Key::Up:
            case static_cast<char32_t>('k'):
                cursor = (cursor == 0) ? static_cast<int>(items.size()) - 1
                                       : cursor - 1;
                keep_visible();
                return true;
                break;

            case input::Key::Down:
            case static_cast<char32_t>('j'):
                cursor = (cursor == static_cast<int>(items.size()) - 1)
                             ? 0
                             : cursor + 1;
                keep_visible();
                return true;
                break;

            case static_cast<char32_t>(' '):
                checked[cursor] = !checked[cursor];
                return true;
                break;

            default:
                return false;
        }
    }
};
}  // namespace trux::component
