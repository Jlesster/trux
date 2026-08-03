#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/style.hpp"

#include <string_view>
#include <vector>

namespace trux::component {
struct Menu {
    explicit Menu(std::vector<std::string>& items) : items(to_views(items)) {}

    std::vector<std::string_view> items{};

    style::Style   selected_style{};
    ComponentFlags flags{};

    int selected = 0;

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;

        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd);
            content = inset(area);
        }

        auto pos = content.position();
        for(auto item : items) {
            cmd.push(renderer::DrawText{
                .position = pos,
                .text     = item,
            });
            pos.y++;
        }
    }
};
}  // namespace trux::component
