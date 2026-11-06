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
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace trux::component {

/// A vertically-scrollable checklist: each item shows a `[x]`/`[ ]`
/// glyph reflecting its checked state, with `cursor` indicating which
/// row Up/Down/j/k move and Space toggles.
///
/// A default-constructed Checkbox is unbound: it draws only its border
/// and consumes no events. The item range is model data and stays
/// external: it is held by pointer and must outlive the widget. The
/// checked state is owned by the checkbox and sized to the item count
/// (grown on bind() and on demand); read and write it through
/// is_checked(), set_checked() and toggle(). `cursor` and `offset` are
/// plain owned fields.
struct Checkbox {
    Checkbox() = default;

    /// Binds the checkbox to an external `items` range, with every item
    /// initially unchecked. `proj` maps an item to something
    /// convertible to std::string_view.
    template <typename R, typename Proj = std::identity>
        requires(!std::same_as<std::remove_cvref_t<R>, Checkbox>)
    explicit Checkbox(R& items, Proj proj = {}) {
        bind(items, std::move(proj));
    }

    /// (Re)binds to `items`, keeping cursor, offset and any existing
    /// checked state; the state grows (unchecked) to cover new items.
    /// Call reset_checked() afterwards if the new range is unrelated to
    /// the old one.
    template <typename R, typename Proj = std::identity>
    void bind(R& items, Proj proj = {}) {
        view.bind(items, std::move(proj));
        if(checked_.size() < size()) checked_.resize(size(), false);
    }

    /// Number of bound items; 0 while unbound.
    size_t size() const { return view.size(); }

    /// Type-erased view of the bound range and projection.
    ItemView view;

    /// Whether the item at `index` is checked. Indices past the stored
    /// state (including out-of-range ones) read as unchecked.
    bool is_checked(size_t index) const {
        return index < checked_.size() && checked_[index];
    }

    /// Sets the checked state of the item at `index`, growing the
    /// stored state (unchecked for the gap) as needed. Ignored while
    /// `index` is beyond the bound items.
    void set_checked(size_t index, bool value) {
        if(index >= size()) return;
        if(index >= checked_.size())
            checked_.resize(std::max(size(), index + 1), false);
        checked_[index] = value;
    }

    /// Flips the checked state of the item at `index`; returns the new
    /// state (false if `index` is beyond the bound items).
    bool toggle(size_t index) {
        if(index >= size()) return false;
        set_checked(index, !is_checked(index));
        return is_checked(index);
    }

    /// Unchecks every item and resizes the state to the current item
    /// count.
    void reset_checked() { checked_.assign(size(), false); }

    /// Number of checked items.
    size_t checked_count() const {
        return static_cast<size_t>(
            std::count(checked_.begin(), checked_.end(), true));
    }

    /// Style applied to the item under the cursor.
    style::Style cursor_style{style::Style::Italic};
    ComponentFlags flags{};
    style::Color border_color{255, 255, 255, 255};
    /// Border color used when unfocused (see component::BorderColor).
    std::optional<style::Color> border_color_fixed;
    /// Border color used while focused; falls back to
    /// `border_color_fixed` when unset.
    std::optional<style::Color> border_color_active;
    /// Foreground for items and glyphs; unset keeps the default.
    std::optional<style::Color> text_color;
    /// Foreground for the item under the cursor; falls back to `text_color`.
    std::optional<style::Color> selection_color;

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

            bool on = is_checked(i);
            bool at_cursor = static_cast<int>(i) == cursor;
            auto fg =
                (at_cursor && selection_color) ? selection_color : text_color;
            renderer::DrawText glyph{
                .position = pos,
                .text = on ? glyph_on : glyph_off,
            };
            if(fg) glyph.fg = *fg;
            cmd.push(glyph);
            view.draw(i,
                      {pos.x + 4, pos.y},
                      at_cursor ? cursor_style : style::Style::None,
                      cmd,
                      fg);
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

            case static_cast<char32_t>(' '):
                toggle(static_cast<size_t>(cursor));
                return true;

            default: return false;
        }
    }

private:
    /// One entry per item (grown by bind()/set_checked()); may be
    /// longer than size() if the bound range shrinks.
    std::vector<bool> checked_;
};
}  // namespace trux::component
