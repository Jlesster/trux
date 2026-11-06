/// @file dropdown.hpp
/// @brief Dropdown: a closed/open selector — collapsed it shows only
///        the current selection, and Enter/Space expands it into a
///        scrollable option list.

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/component/item_view.hpp"
#include "trux/input/event.hpp"
#include "trux/input/key.hpp"
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

namespace trux::component {

/// A collapsible single-selection list. While closed (the default),
/// only the currently selected option is drawn. Enter or Space opens
/// it, revealing a vertically-scrollable list where Up/Down (or j/k)
/// move the selection, and Enter/Space/Escape close it again.
///
/// A default-constructed Dropdown is unbound: it draws `--` and
/// consumes no events. The option range is held by pointer and must
/// outlive the dropdown. `selection`, `open` and `offset` are plain
/// owned fields.
struct Dropdown {
    Dropdown() = default;

    /// Binds the dropdown to an external `options` range. `proj` maps
    /// an option to something convertible to std::string_view.
    template <typename R, typename Proj = std::identity>
        requires(!std::same_as<std::remove_cvref_t<R>, Dropdown>)
    explicit Dropdown(R& options, Proj proj = {}) {
        bind(options, std::move(proj));
    }

    /// (Re)binds to `options`, keeping selection, open state and offset.
    template <typename R, typename Proj = std::identity>
    void bind(R& options, Proj proj = {}) {
        view.bind(options, std::move(proj));
    }

    /// Number of bound options; 0 while unbound.
    size_t size() const { return view.size(); }

    /// Type-erased view of the bound range and projection.
    ItemView view;

    /// Index of the currently selected option. Owned by the dropdown,
    /// so it can drift from the bound range when options are removed;
    /// it is re-clamped by build(), handle() and clamp().
    mutable int selection{0};
    /// Whether the option list is currently expanded.
    bool open{false};

    /// Style applied to the highlighted option while open.
    style::Style highlight_style{style::Style::Reverse};
    ComponentFlags flags{};
    style::Color border_color{255, 255, 255, 255};
    /// Border color used when unfocused (see component::BorderColor).
    std::optional<style::Color> border_color_fixed;
    /// Border color used while focused; falls back to
    /// `border_color_fixed` when unset.
    std::optional<style::Color> border_color_active;
    /// Foreground for options; unset keeps the default.
    std::optional<style::Color> text_color;
    /// Foreground for the highlighted option while open; falls back to
    /// `text_color`.
    std::optional<style::Color> selection_color;
    /// Height of the list while open (border included). Unset fits every
    /// option; always clamped to the room on screen, so a long list is
    /// capped there and scrolls. Use layout::kFill to take all of it.
    std::optional<int> expanded_height;

    /// Rows the dropdown occupies while closed: the selection line, plus
    /// the border's top and bottom when one is set.
    int collapsed_height() const { return active_border(flags) ? 3 : 1; }

    /// Rows used while open: `expanded_height` if set, otherwise enough
    /// for every option (plus the border rows, if any).
    int open_height() const {
        return expanded_height.value_or(std::max(
            collapsed_height(),
            static_cast<int>(size()) + (active_border(flags) ? 2 : 0)));
    }

    /// While open, Tab goes to handle() (which swallows it) instead of
    /// moving focus away and leaving the list expanded.
    bool captures_tab() const { return open; }

    /// Index of the first visible option while open; clamped during build().
    mutable int offset = 0;
    /// Content height from the most recent build(), used by
    /// handle()'s keep_visible() to scroll the selection into view.
    mutable int last_height{0};

    /// Pulls `selection` back into [0, size()-1] (0 when empty).
    /// Called automatically by build() and handle().
    void clamp() const {
        selection =
            std::clamp(selection, 0, std::max(0, static_cast<int>(size()) - 1));
    }

    /// While closed, draws only the selected option's text (or `--`
    /// if there are no options). While open, clamps `offset` and draws
    /// the visible slice of options, highlighting `selection`.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        // The split always sees the closed footprint, so toggling never
        // moves a sibling.
        area.request_size({.height = collapsed_height()});

        // Open: paint a taller frame on the overlay layer, over whatever
        // it lands on. It grows down unless that doesn't fit and there is
        // more room above, in which case it grows up from our bottom edge.
        auto frame = area;
        std::optional<renderer::OverlayScope> overlay;
        if(open) {
            auto [x, y] = area.position();
            auto [w, h] = area.size();
            auto screen = area.root_rect();
            int want = open_height();
            int below = screen.position.y + screen.size.height - y;
            int above = y + h - screen.position.y;
            bool up = want > below && above > below;
            int rows = std::clamp(want, h, std::max(h, up ? above : below));
            frame = layout::Region{{x, up ? y + h - rows : y}, {w, rows}};
            overlay.emplace(cmd, frame);
        }

        auto content = frame;
        if(auto border = active_border(flags)) {
            build_border(frame, *border, cmd, border_color);
            content = inset(frame);
        }

        clamp();
        auto pos = content.position();
        if(!open) {
            if(size() == 0 || static_cast<size_t>(selection) >= size()) {
                cmd.push(renderer::DrawText{
                    .position = pos,
                    .text = std::string_view{"--"},
                });
            } else {
                view.draw(static_cast<size_t>(selection),
                          pos,
                          style::Style::None,
                          cmd,
                          text_color);
            }
            return;
        }
        auto [w, h] = content.size();
        last_height = h;

        int max_offset = std::max(0, static_cast<int>(size()) - h);
        offset = std::clamp(offset, 0, max_offset);

        size_t begin = static_cast<size_t>(offset);
        size_t end =
            std::min(size(), begin + static_cast<size_t>(std::max(h, 0)));
        for(size_t i = begin; i < end; i++) {
            view.draw(i,
                      pos,
                      (static_cast<int>(i) == selection) ? highlight_style
                                                         : style::Style::None,
                      cmd,
                      (static_cast<int>(i) == selection && selection_color)
                          ? selection_color
                          : text_color);
            pos.y++;
        }
    }

    /// While closed: Enter/Space opens the list. While open: Up/k and
    /// Down/j move `selection` (wrapping) and scroll to keep it visible;
    /// Enter/Space/Escape close it; any other key is consumed without
    /// effect. Returns whether the event was consumed.
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(size() == 0) return false;
        clamp();

        if(!open) {
            switch(event.code) {
                case input::Key::Enter:
                case static_cast<char32_t>(' '): open = true; return true;
                default: return false;
            }
        }

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

            case input::Key::Enter:
            case static_cast<char32_t>(' '):
            case input::Key::Escape: open = false; return true;

            default: return true;
        }
    }
};
}  // namespace trux::component
