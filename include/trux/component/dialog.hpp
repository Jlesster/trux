/// @file dialog.hpp
/// @brief Dialog: a modal overlay wrapping an inner component in a
///        centered, optionally-titled and bordered box; typically
///        pushed with `modal = true` (see Renderer::push()).

#pragma once

#include "trux/input/event.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

#include <algorithm>
#include <string_view>
#include <trux/component/border.hpp>
#include <trux/component/component.hpp>
#include <trux/style/style.hpp>

namespace trux::component {

/// The `size`-sized Region centered within `root`, clamped so it
/// never exceeds `root`'s bounds.
[[nodiscard]]
inline layout::Region centered(layout::Region root, layout::Size size) {
    auto [rw, rh] = root.size();
    auto pos      = root.position();
    int  w        = std::min(size.width, rw);
    int  h        = std::min(size.height, rh);
    return layout::Region{
        {pos.x + (rw - w) / 2, pos.y + (rh - h) / 2},
        {w,                    h                   }
    };
}

/// A modal box that fills a fixed `size` centered within whatever
/// area it's built into, drawing an opaque background, an optional
/// border, and an optional title above its wrapped `content`. Escape
/// closes the dialog via `open` if set; all other events are
/// forwarded to `content` when it's input::Handleable.
///
/// @tparam T Wrapped component type satisfying ComponentType.
template <ComponentType T> struct Dialog {
    /// Size of the dialog box, regardless of the root area it's built into.
    layout::Size     size{40, 7};
    /// Optional title drawn (bold) above the content; omitted if empty.
    std::string_view title{};
    ComponentFlags   flags{};
    style::Color     border_color{255, 255, 255, 255};
    /// If non-null, set to false when Escape closes the dialog.
    bool*            open = nullptr;

    T content;

    /// Centers the dialog within `root`, fills it opaquely, draws the
    /// optional border and title, and builds `content` into the
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

            inner = layout::Region{
                {inner.position().x, inner.position().y + 1 },
                {inner.size().width, inner.size().height - 1}
            };
        }
        content.build(inner, cmd);
    }

    /// Escape closes the dialog (via `open`, if set) and is always
    /// consumed. All other events are forwarded to `content.handle()`
    /// if `T` is input::Handleable, otherwise consumed without effect
    /// (so events never fall through to whatever is behind a modal
    /// dialog with non-handleable content).
    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(event.kind == input::EventKind::Key &&
           event.code == input::Key::Escape) {
            if(open) *open = false;
            return true;
        }
        if constexpr(input::Handleable<T>) return content.handle(event);
        else return true;
    }
};
}  // namespace trux::component
