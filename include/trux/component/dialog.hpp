/// @file dialog.hpp
/// @brief Dialog: a modal overlay with a centered, optionally-titled
///        and bordered box containing an embedded TextInput; typically
///        pushed with `modal = true` (see Renderer::push()).

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component.hpp"
#include "trux/component/text_input.hpp"
#include "trux/input/event.hpp"
#include "trux/input/key.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <functional>
#include <string>

namespace trux::component {

/// The `size`-sized Region centered within `root`, clamped so it
/// never exceeds `root`'s bounds.
[[nodiscard]]
inline layout::Region centered(layout::Region root, layout::Size size) {
    auto [rw, rh] = root.size();
    auto pos = root.position();
    int w = std::min(size.width, rw);
    int h = std::min(size.height, rh);
    return layout::Region{{pos.x + (rw - w) / 2, pos.y + (rh - h) / 2}, {w, h}};
}

/// A modal box that fills a fixed `size` centered within whatever
/// area it's built into, drawing an opaque background, an optional
/// border, an optional title, and an embedded TextInput below it.
///
/// Everything is owned: `open` is the dialog's own visibility flag,
/// and the text lives in `input` (TextInput stays a standalone
/// primitive; Dialog just holds one), so `dialog.input.value` and
/// `dialog.input.cursor` are readable and writable directly, e.g. from
/// `on_confirm`. Escape closes the dialog; Enter runs `on_confirm`
/// and then closes it; every other event goes to `input`.
struct Dialog {
    /// Size of the dialog box, regardless of the root area it's built into.
    layout::Size size{40, 7};
    /// Optional title drawn (bold) above the input; omitted if empty.
    std::string title{};
    ComponentFlags flags{};
    style::Color border_color{255, 255, 255, 255};
    /// Whether the dialog is showing. Cleared by Escape and Enter;
    /// callers set it to show the dialog and skip pushing it when false.
    bool open = false;
    /// Called when Enter is pressed, before the dialog closes.
    std::function<void()> on_confirm{};

    /// The embedded text field. Its `flags` and `border_color` are
    /// independent of the dialog's own.
    TextInput input;

    /// Centers the dialog within `root`, fills it opaquely, draws the
    /// optional border and title, and builds `input` into the
    /// remaining space.
    void build(layout::Region root, renderer::DrawCommandBuffer& cmd) const {
        auto area = centered(root, size);
        cmd.push(renderer::Fill{area, renderer::Cell{}});

        auto inner = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            inner = inset(area);
        }

        if(!title.empty()) {
            cmd.push(renderer::DrawText{
                inner.position(), title, style::Style::Bold});

            inner =
                layout::Region{{inner.position().x, inner.position().y + 1},
                               {inner.size().width, inner.size().height - 1}};
        }
        input.build(inner, cmd);
    }

    /// Escape closes the dialog and is always consumed. Enter runs
    /// `on_confirm`, closes the dialog, and is always consumed. All
    /// other events are forwarded to `input.handle()`.
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(event.kind == input::EventKind::Key &&
           event.code == input::Key::Escape) {
            open = false;
            return true;
        }
        if(event.code == input::Key::Enter) {
            if(on_confirm) on_confirm();
            open = false;
            return true;
        }
        return input.handle(event);
    }
};
}  // namespace trux::component
