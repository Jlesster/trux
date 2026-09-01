#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace trux::component {
struct List {
    List(std::vector<std::string>& items, int& scroll_offset)
        : items(items), scroll_offset(scroll_offset) {}

    std::vector<std::string>& items;

    ComponentFlags flags{};
    style::Color   border_color{255, 255, 255, 255};
    int&           scroll_offset;

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;

        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h] = content.size();

        int max_offset = std::max(0, static_cast<int>(items.size()) - h);
        scroll_offset  = std::clamp(scroll_offset, 0, max_offset);

        auto   pos = content.position();
        size_t begin =
            static_cast<size_t>(std::clamp(scroll_offset, 0, std::max(h, 0)));
        size_t end =
            std::min(items.size(), begin + static_cast<size_t>(std::max(h, 0)));
        for(size_t i = begin; i < end; i++) {
            cmd.push(renderer::DrawText{.position = pos, .text = items[i]});
            pos.y++;
        }
    }
};
}  // namespace trux::component
