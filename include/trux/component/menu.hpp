#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <string>
#include <vector>

namespace trux::component {
struct Menu {
    Menu(std::vector<std::string>& items, int& selected, int& scroll_offset)
        : items(items), selected(selected), scroll_offset(scroll_offset) {}

    std::vector<std::string>& items;

    style::Style   selected_style{style::Style::Italic};
    ComponentFlags flags{};

    int& scroll_offset;
    int& selected;

    mutable int last_height{0};

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd);
            content = inset(area);
        }

        auto [w, h] = content.size();
        last_height = h;

        int max_offset = std::max(0, static_cast<int>(items.size()) - h);
        scroll_offset  = std::clamp(scroll_offset, 0, max_offset);

        auto   pos   = content.position();
        size_t begin = static_cast<size_t>(scroll_offset);
        size_t end =
            std::min(items.size(), begin + static_cast<size_t>(std::max(h, 0)));
        for(size_t i = begin; i < end; i++) {
            cmd.push(renderer::DrawText{
                .position = pos,
                .text     = items[i],
                .style    = (static_cast<int>(i) == selected) ? selected_style
                                                              : style::Style::None,
            });
            pos.y++;
        }
    }

    [[nodiscard]]
    bool handle(const input::Event& event) {
        std::fprintf(stderr, "Menu::handle code=%u\n", event.code);
        std::fprintf(stderr, "selection=%u\n", selected);
        if(items.empty()) return false;
        auto keep_visible = [this] {
            if(selected < scroll_offset) {
                scroll_offset = selected;
            } else if(last_height > 0 &&
                      selected >= scroll_offset + last_height) {
                scroll_offset = selected - last_height + 1;
            }
        };
        switch(event.code) {
            case input::Key::Up:
            case static_cast<char32_t>('k'):
                selected = (selected == 0) ? static_cast<int>(items.size()) - 1
                                           : selected - 1;
                keep_visible();
                return true;
                break;

            case input::Key::Down:
            case static_cast<char32_t>('j'):
                selected = (selected == static_cast<int>(items.size()) - 1)
                               ? 0
                               : selected + 1;
                keep_visible();
                return true;
                break;

            case input::Key::Enter:
                return true;
                break;
            default:
                return false;
                break;
        }
    }
};
}  // namespace trux::component
