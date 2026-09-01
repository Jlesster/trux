#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/layout/region.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

#include <optional>

namespace trux::component {

enum BorderDir : uint8_t { North = 1, South = 2, East = 4, West = 8 };

struct JunctionSet {
    char32_t horizontal, vertical;
    char32_t top_left, top_right, bottom_left, bottom_right;
    char32_t t_down, t_up, t_right, t_left, cross;
};

[[nodiscard]]
inline const JunctionSet& junctions_for(Flag style) {
    static const JunctionSet single{
        U'─', U'│', U'┌', U'┐', U'└', U'┘', U'┬', U'┴', U'├', U'┤', U'┼'};
    static const JunctionSet& rounded{
        U'─', U'│', U'╭', U'╮', U'╰', U'╯', U'┬', U'┴', U'├', U'┤', U'┼'};
    static const JunctionSet dbl{
        U'═', U'║', U'╔', U'╗', U'╚', U'╝', U'╦', U'╩', U'╠', U'╣', U'╬'};
    if(style == Flag::BorderDouble) return dbl;
    if(style == Flag::BorderRounded) return rounded;
    return single;
};

struct BorderGlyphs {
    char32_t top_left, top_right, bottom_left, bottom_right;
    char32_t horizontal, vertical;
};

[[nodiscard]]
inline char32_t glyph_for_mask(const JunctionSet& j, uint8_t mask) {
    switch(mask) {
        case North | South:
            return j.vertical;
        case East | West:
            return j.horizontal;
        case South | East:
            return j.top_left;
        case South | West:
            return j.top_right;
        case North | East:
            return j.bottom_left;
        case North | West:
            return j.bottom_right;
        case South | East | West:
            return j.t_down;
        case North | East | West:
            return j.t_up;
        case North | South | East:
            return j.t_right;
        case North | South | West:
            return j.t_left;
        case North | South | East | West:
            return j.cross;
        default:
            return U'\0';
    }
}

struct Classified {
    bool    is_border{false};
    Flag    style{Flag::BorderSingle};
    uint8_t mask{0};
};

[[nodiscard]]
inline Classified classify_border_glyph(char32_t glyph) {
    struct Entry {
        char32_t ch;
        Flag     style;
        uint8_t  mask;
    };
    static constexpr Entry table[] = {
        {U'─', Flag::BorderSingle,  East | West                },
        {U'│', Flag::BorderSingle,  North | South              },
        {U'┌', Flag::BorderSingle,  South | East               },
        {U'┐', Flag::BorderSingle,  South | West               },
        {U'└', Flag::BorderSingle,  North | East               },
        {U'┘', Flag::BorderSingle,  North | West               },
        {U'┬', Flag::BorderSingle,  South | East | West        },
        {U'┴', Flag::BorderSingle,  North | East | West        },
        {U'├', Flag::BorderSingle,  North | South | East       },
        {U'┤', Flag::BorderSingle,  North | South | West       },
        {U'┼', Flag::BorderSingle,  North | South | East | West},
        {U'╭', Flag::BorderRounded, South | East               },
        {U'╮', Flag::BorderRounded, South | West               },
        {U'╰', Flag::BorderRounded, North | East               },
        {U'╯', Flag::BorderRounded, North | West               },
        {U'═', Flag::BorderDouble,  East | West                },
        {U'║', Flag::BorderDouble,  North | South              },
        {U'╔', Flag::BorderDouble,  South | East               },
        {U'╗', Flag::BorderDouble,  South | West               },
        {U'╚', Flag::BorderDouble,  North | East               },
        {U'╝', Flag::BorderDouble,  North | West               },
        {U'╦', Flag::BorderDouble,  South | East | West        },
        {U'╩', Flag::BorderDouble,  North | East | West        },
        {U'╠', Flag::BorderDouble,  North | South | East       },
        {U'╣', Flag::BorderDouble,  North | South | West       },
        {U'╬', Flag::BorderDouble,  North | South | East | West},
    };
    for(auto& e : table)
        if(e.ch == glyph) return {true, e.style, e.mask};
    return {};
}

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
                         renderer::DrawCommandBuffer& cmd,
                         style::Color color = {255, 255, 255, 255}) {
    auto [w, h] = area.size();
    if(w < 2 || h < 2) return;

    auto pos    = area.position();
    auto g      = glyphs_for(border);
    int  right  = pos.x + w - 1;
    int  bottom = pos.y + h - 1;

    auto cell = [&](char32_t c) {
        return renderer::Cell{.glyph = c, .foreground = color};
    };

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
