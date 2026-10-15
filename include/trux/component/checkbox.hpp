/// @file checkbox.hpp
/// @brief Checkbox: a scrollable list of independently toggleable
///        items, with a movable cursor and space-to-toggle input.

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/component/item_view.hpp"
#include "trux/input/event.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace trux::component {

/// A vertically-scrollable checklist: each item shows a `[x]`/`[ ]`
/// glyph reflecting the corresponding entry in the bound `checked`
/// vector, with `cursor` indicating which row Up/Down/j/k move and
/// Space toggles.
///
/// A default-constructed Checkbox is unbound: it draws only its border
/// and consumes no events. The item range and the `checked` vector are
/// model data and stay external: both are held by pointer and must
/// outlive the widget. `checked` should be the same length as the
/// items; rows past its end draw as unchecked and can't be toggled.
/// `cursor` and `offset` are plain owned fields.
struct Checkbox {
    Checkbox() = default;

    /// Binds the checkbox to an external `items` range and `checked`
    /// vector. `proj` maps an item to something convertible to
    /// std::string_view.
    template <typename R, typename Proj = std::identity>
        requires(!std::same_as<std::remove_cvref_t<R>, Checkbox>)
    Checkbox(R& items, std::vector<bool>& checked, Proj proj = {}) {
        bind(items, checked, std::move(proj));
    }

    /// (Re)binds to `items` and `checked`, keeping cursor and offset.
    template <typename R, typename Proj = std::identity>
    void bind(R& items, std::vector<bool>& checked_state, Proj proj = {}) {
        view.bind(items, std::move(proj));
        checked = &checked_state;
    }

    /// Number of bound items; 0 while unbound.
    size_t size() const { return view.size(); }

    /// Type-erased view of the bound range and projection.
    ItemView view;
    /// Checked state, one entry per item; null while unbound.
    std::vector<bool>* checked = nullptr;

    /// Style applied to the item under the cursor.
    style::Style cursor_style{style::Style::Italic};
    ComponentFlags flags{};
    style::Color border_color{255, 255, 255, 255};

    /// Index of the item the cursor is on. Owned by the checkbox, so
    /// it can drift from the bound range when items are removed; it is
    /// re-clamped by build(), handle() and clamp().
    mutable int cursor = 0;
    /// Index of the first visible item; clamped during build().
    mutable int offset = 0;
    /// Content height from the most recent build(), used by
    /// handle()'s keep_visible() to scroll the cursor into view.
    mutable int last_height{0};

    /// Pulls `cursor` back into [0, size()-1] (0 when empty).
    /// Called automatically by build() and handle().
    void clamp() const {
        cursor =
            std::clamp(cursor, 0, std::max(0, static_cast<int>(size()) - 1));
    }

    /// Clamps `offset`, then draws the visible slice of items with
    /// their checked-state glyph, highlighting the row at `cursor`.
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
            static constexpr std::string_view glyph_on = "[x] ";
            static constexpr std::string_view glyph_off = "[ ] ";

            bool on = checked && i < checked->size() && (*checked)[i];
            cmd.push(renderer::DrawText{
                .position = pos,
                .text = on ? glyph_on : glyph_off,
            });
            view.draw(i,
                      {pos.x + 4, pos.y},
                      (static_cast<int>(i) == cursor) ? cursor_style
                                                      : style::Style::None,
                      cmd);
            pos.y++;
        }
    }

    /// Up/k and Down/j move `cursor` (wrapping) and scroll to keep it
    /// visible; Space toggles the checked state at `cursor`. Returns
    /// whether the event was consumed.
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(size() == 0) return false;
        clamp();
        auto keep_visible = [this] {
            if(cursor < offset) {
                offset = cursor;
            } else if(last_height > 0 && cursor >= offset + last_height) {
                offset = cursor - last_height + 1;
            }
        };
        switch(event.code) {
            case input::Key::Up:
            case static_cast<char32_t>('k'):
                cursor =
                    (cursor == 0) ? static_cast<int>(size()) - 1 : cursor - 1;
                keep_visible();
                return true;

            case input::Key::Down:
            case static_cast<char32_t>('j'):
                cursor =
                    (cursor == static_cast<int>(size()) - 1) ? 0 : cursor + 1;
                keep_visible();
                return true;

            case static_cast<char32_t>(' '): {
                auto i = static_cast<size_t>(cursor);
                if(checked && i < checked->size())
                    (*checked)[i] = !(*checked)[i];
                return true;
            }

            default: return false;
        }
    }
};
}  // namespace trux::component
