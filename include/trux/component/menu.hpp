/// @file menu.hpp
/// @brief Menu: a scrollable, single-selection list with a highlighted
///        current item and Up/Down/Enter navigation.

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/input/event.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <functional>
#include <vector>

namespace trux::component {

/// A vertically-scrollable, single-selection menu: Up/Down (or j/k)
/// move `selected` (wrapping), and Enter is reported as consumed so
/// callers can check `event.code == input::Key::Enter` after handle()
/// returns true to detect an activation.
///
/// @tparam R    Range of items to display.
/// @tparam Proj Projection from an item to something DrawText accepts.
///              Defaults to std::identity.
template <typename R, typename Proj = std::identity> struct Menu {
    /// Binds the menu to external `items`, `selected`, and
    /// `scroll_offset` storage, all kept by reference.
    Menu(R& items, int& selected, int& scroll_offset, Proj proj = {})
        : items(items), selected(selected), scroll_offset(scroll_offset),
          proj(proj) {}

    R&                        items;
    Proj                      proj;
    /// Per-item text style overrides, indexed like `items`; see
    /// set_style()/style_for(). Combined with selected_style for the
    /// currently selected item.
    std::vector<style::Style> item_styles;

    /// Style OR'd onto the selected item's own style.
    style::Style   selected_style{style::Style::Italic};
    ComponentFlags flags{};
    style::Color   border_color{255, 255, 255, 255};

    /// Index of the first visible item; clamped during build().
    int& scroll_offset;
    /// Index of the currently selected item.
    int& selected;

    /// Content height from the most recent build(), used by
    /// handle()'s keep_visible() to scroll the selection into view.
    mutable int last_height{0};

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

    /// Clamps `scroll_offset`, then draws the visible slice of items,
    /// combining selected_style into the selected row's style.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h] = content.size();
        last_height = h;

        int max_offset = std::max(0, static_cast<int>(items.size()) - h);
        scroll_offset  = std::clamp(scroll_offset, 0, max_offset);

        auto   pos   = content.position();
        size_t begin = static_cast<size_t>(scroll_offset);
        size_t end =
            std::min(items.size(), begin + static_cast<size_t>(std::max(h, 0)));
        for(size_t i = begin; i < end; i++) {
            cmd.push(renderer::DrawText{
                .position = pos,
                .text     = std::invoke(proj, items[i]),
                .style    = (static_cast<int>(i) == selected)
                                ? (style_for(i) | selected_style)
                                : style_for(i),
            });
            pos.y++;
        }
    }

    /// Up/k and Down/j move `selected` (wrapping) and scroll to keep
    /// it visible; Enter is consumed without changing state (callers
    /// detect activation by checking `event.code` after a true
    /// return). Returns whether the event was consumed.
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(items.empty()) return false;
        auto keep_visible = [this] {
            if(selected < scroll_offset) {
                scroll_offset = selected;
            } else if(last_height > 0 &&
                      selected >= scroll_offset + last_height) {
                scroll_offset = selected - last_height + 1;
            }
        };
        switch(event.code) {
            case input::Key::Up:
            case static_cast<char32_t>('k'):
                selected = (selected == 0) ? static_cast<int>(items.size()) - 1
                                           : selected - 1;
                keep_visible();
                return true;
                break;

            case input::Key::Down:
            case static_cast<char32_t>('j'):
                selected = (selected == static_cast<int>(items.size()) - 1)
                               ? 0
                               : selected + 1;
                keep_visible();
                return true;
                break;

            case input::Key::Enter:
                return true;
                break;
            default:
                return false;
                break;
        }
    }
};
}  // namespace trux::component
