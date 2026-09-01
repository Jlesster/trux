#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/input/event.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <string>
#include <vector>
namespace trux::component {

struct Checkbox {
    Checkbox(std::vector<std::string>& items,
             std::vector<bool>&        checked,
             int&                      cursor,
             int&                      scroll_offset)
        : items(items), checked(checked), cursor(cursor),
          scroll_offset(scroll_offset) {}
    std::vector<std::string>& items;
    std::vector<bool>&        checked;

    style::Style   cursor_style{style::Style::Italic};
    ComponentFlags flags{};
    style::Color   border_color{255, 255, 255, 255};

    int&        cursor;
    int&        scroll_offset;
    mutable int last_height{0};

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h]    = content.size();
        last_height    = h;
        int max_offset = std::max(0, static_cast<int>(items.size()) - h);
        scroll_offset  = std::clamp(scroll_offset, 0, max_offset);

        auto   pos   = content.position();
        size_t begin = static_cast<size_t>(scroll_offset);
        size_t end =
            std::min(items.size(), begin + static_cast<size_t>(std::max(h, 0)));

        for(size_t i = begin; i < end; i++) {
            static constexpr std::string_view glyph_on  = "[x] ";
            static constexpr std::string_view glyph_off = "[ ] ";

            cmd.push(renderer::DrawText{
                .position = pos,
                .text     = checked[i] ? glyph_on : glyph_off,
            });
            cmd.push(renderer::DrawText{
                .position = {pos.x + 4, pos.y},
                .text     = items[i],
                .style = (static_cast<int>(i) == cursor) ? cursor_style
                                                         : style::Style::None,
            });
            pos.y++;
        }
    }

    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(items.empty()) return false;
        auto keep_visible = [this] {
            if(cursor < scroll_offset) {
                scroll_offset = cursor;
            } else if(last_height > 0 &&
                      cursor >= scroll_offset + last_height) {
                scroll_offset = cursor - last_height + 1;
            }
        };
        switch(event.code) {
            case input::Key::Up:
            case static_cast<char32_t>('k'):
                cursor = (cursor == 0) ? static_cast<int>(items.size()) - 1
                                       : cursor - 1;
                keep_visible();
                return true;
                break;

            case input::Key::Down:
            case static_cast<char32_t>('j'):
                cursor = (cursor == static_cast<int>(items.size()) - 1)
                             ? 0
                             : cursor + 1;
                keep_visible();
                return true;
                break;

            case static_cast<char32_t>(' '):
                checked[cursor] = !checked[cursor];
                return true;
                break;

            default:
                return false;
        }
    }
};
}  // namespace trux::component
