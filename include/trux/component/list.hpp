#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <functional>
#include <vector>

namespace trux::component {

template <typename R, typename Proj = std::identity> struct List {
    List(R& items, int& scroll_offset, Proj proj = {})
        : items(items), scroll_offset(scroll_offset), proj(proj) {}

    R&                        items;
    Proj                      proj;
    std::vector<style::Style> item_styles;

    ComponentFlags flags{};
    style::Color   border_color{255, 255, 255, 255};
    int&           scroll_offset;

    void set_style(size_t index, style::Style style) {
        if(index >= item_styles.size())
            item_styles.resize(items.size(), style::Style::None);
        item_styles[index] = style;
    }

    style::Style style_for(size_t index) const {
        return index < item_styles.size() ? item_styles[index]
                                          : style::Style::None;
    }

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
            cmd.push(renderer::DrawText{.position = pos,
                                        .text     = std::invoke(proj, items[i]),
                                        .style    = style_for(i)});
            pos.y++;
        }
    }
};

}  // namespace trux::component
