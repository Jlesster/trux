#pragma once

#include <optional>
#include <trux/component/component_flags.hpp>
#include <trux/layout/region.hpp>
#include <trux/renderer/draw_command_buffer.hpp>

namespace trux::component {

struct BorderGlyphs {
    char32_t top_left, top_right, bottom_left, bottom_right;
    char32_t horizontal, vertical;
};

[[nodiscard]]
inline BorderGlyphs glyphs_for(Flag border) {
    switch(border) {
        case Flag::BorderRounded:
            return {U'╭', U'╮', U'╰', U'╯', U'─', U'│'};
        case Flag::BorderDouble:
            return {U'╔', U'╗', U'╚', U'╝', U'═', U'║'};
        case Flag::BorderBlock:
            return {U'█', U'█', U'█', U'█', U'█', U'█'};
        default:  // BorderSingle
            return {U'┌', U'┐', U'└', U'┘', U'─', U'│'};
    }
}

[[nodiscard]]
inline std::optional<Flag> active_border(const ComponentFlags& flags) {
    if(flags.has(Flag::BorderRounded)) return Flag::BorderRounded;
    if(flags.has(Flag::BorderDouble)) return Flag::BorderDouble;
    if(flags.has(Flag::BorderBlock)) return Flag::BorderBlock;
    if(flags.has(Flag::BorderSingle)) return Flag::BorderSingle;
    return std::nullopt;
}

inline void build_border(layout::Region               area,
                         Flag                         border,
                         renderer::DrawCommandBuffer& cmd) {
    auto [w, h] = area.size();
    if(w < 2 || h < 2) return;

    auto pos    = area.position();
    auto g      = glyphs_for(border);
    int  right  = pos.x + w - 1;
    int  bottom = pos.y + h - 1;

    auto cell = [](char32_t c) { return renderer::Cell{.glyph = c}; };

    cmd.push(renderer::DrawCell{
        {pos.x, pos.y},
        cell(g.top_left)
    });
    cmd.push(renderer::DrawCell{
        {right, pos.y},
        cell(g.top_right)
    });
    cmd.push(renderer::DrawCell{
        {pos.x, bottom},
        cell(g.bottom_left)
    });
    cmd.push(renderer::DrawCell{
        {right, bottom},
        cell(g.bottom_right)
    });

    for(int x = pos.x + 1; x < right; x++) {
        cmd.push(renderer::DrawCell{
            {x, pos.y},
            cell(g.horizontal)
        });
        cmd.push(renderer::DrawCell{
            {x, bottom},
            cell(g.horizontal)
        });
    }
    for(int y = pos.y + 1; y < bottom; y++) {
        cmd.push(renderer::DrawCell{
            {pos.x, y},
            cell(g.vertical)
        });
        cmd.push(renderer::DrawCell{
            {right, y},
            cell(g.vertical)
        });
    }
}

[[nodiscard]]
inline layout::Region inset(layout::Region area) {
    auto pos  = area.position();
    auto size = area.size();
    return layout::Region{
        {pos.x + 1,      pos.y + 1      },
        {size.width - 2, size.height - 2}
    };
}

}  // namespace trux::component
