/// @file menu.hpp
/// @brief Menu: a scrollable, single-selection list with a highlighted
///        current item and Up/Down/Enter navigation.

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/component/item_view.hpp"
#include "trux/input/event.hpp"
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

/// A vertically-scrollable, single-selection menu: Up/Down (or j/k)
/// move `selection` (wrapping), and Enter is reported as consumed so
/// callers can check `event.code == input::Key::Enter` after handle()
/// returns true to detect an activation.
///
/// A default-constructed Menu is unbound: it draws only its border and
/// consumes no events until bind() (or assignment from a bound Menu).
/// The item range is held by pointer and must outlive the menu.
/// `selection` and `offset` are plain owned fields.
struct Menu {
    Menu() = default;

    /// Binds the menu to an external `items` range. `proj` maps an item
    /// to something convertible to std::string_view.
    template <typename R, typename Proj = std::identity>
        requires(!std::same_as<std::remove_cvref_t<R>, Menu>)
    explicit Menu(R& items, Proj proj = {}) {
        bind(items, std::move(proj));
    }

    /// (Re)binds to `items`, keeping selection and offset.
    template <typename R, typename Proj = std::identity>
    void bind(R& items, Proj proj = {}) {
        view.bind(items, std::move(proj));
    }

    /// Number of bound items; 0 while unbound.
    size_t size() const { return view.size(); }

    /// Type-erased view of the bound range and projection.
    ItemView view;

    /// Per-item text style overrides, indexed like the items; see
    /// set_style()/style_for(). Combined with selected_style for the
    /// currently selected item.
    std::vector<style::Style> item_styles;

    /// Style OR'd onto the selected item's own style.
    style::Style selected_style{style::Style::Italic};
    ComponentFlags flags{};
    style::Color border_color{255, 255, 255, 255};

    /// Index of the first visible item; clamped during build().
    mutable int offset = 0;
    /// Index of the currently selected item. Owned by the menu, so it
    /// can drift from the bound range when items are removed; it is
    /// re-clamped to a valid index by build(), handle() and clamp().
    mutable int selection = 0;

    /// Content height from the most recent build(), used by
    /// handle()'s keep_visible() to scroll the selection into view.
    mutable int last_height{0};

    /// Pulls `selection` back into [0, size()-1] (0 when empty).
    /// Called automatically by build() and handle(); call it directly
    /// after shrinking the bound range if you read `selection` before
    /// the next frame.
    void clamp() const {
        selection =
            std::clamp(selection, 0, std::max(0, static_cast<int>(size()) - 1));
    }

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

    /// Clamps `offset`, then draws the visible slice of items,
    /// combining selected_style into the selected row's style.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h] = content.size();
        last_height = h;
        clamp();

        int max_offset = std::max(0, static_cast<int>(size()) - h);
        offset = std::clamp(offset, 0, max_offset);

        auto pos = content.position();
        size_t begin = static_cast<size_t>(offset);
        size_t end =
            std::min(size(), begin + static_cast<size_t>(std::max(h, 0)));
        for(size_t i = begin; i < end; i++) {
            view.draw(i,
                      pos,
                      (static_cast<int>(i) == selection)
                          ? (style_for(i) | selected_style)
                          : style_for(i),
                      cmd);
            pos.y++;
        }
    }

    /// Up/k and Down/j move `selection` (wrapping) and scroll to keep
    /// it visible; Enter is consumed without changing state (callers
    /// detect activation by checking `event.code` after a true
    /// return). Returns whether the event was consumed.
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(size() == 0) return false;
        clamp();
        auto keep_visible = [this] {
            if(selection < offset) {
                offset = selection;
            } else if(last_height > 0 && selection >= offset + last_height) {
                offset = selection - last_height + 1;
            }
        };
        switch(event.code) {
            case input::Key::Up:
            case static_cast<char32_t>('k'):
                selection = (selection == 0) ? static_cast<int>(size()) - 1
                                             : selection - 1;
                keep_visible();
                return true;

            case input::Key::Down:
            case static_cast<char32_t>('j'):
                selection = (selection == static_cast<int>(size()) - 1)
                                ? 0
                                : selection + 1;
                keep_visible();
                return true;

            case input::Key::Enter: return true;

            default: return false;
        }
    }
};
}  // namespace trux::component
