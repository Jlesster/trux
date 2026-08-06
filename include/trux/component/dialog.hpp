#pragma once

#include "trux/input/event.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <algorithm>
#include <memory>
#include <string_view>
#include <trux/component/border.hpp>
#include <trux/component/component.hpp>
#include <trux/style/style.hpp>

namespace trux::component {

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

template <ComponentType T> struct Dialog {
    layout::Size     size{40, 7};
    std::string_view title{};
    ComponentFlags   flags{};
    bool*            open = nullptr;

    T content;

    void build(layout::Region root, renderer::DrawCommandBuffer& cmd) const {
        auto area = centered(root, size);
        cmd.push(renderer::Fill{area, renderer::Cell{}});

        auto inner = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd);
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
