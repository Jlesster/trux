#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/input/event.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

namespace trux::component {

struct Paragraph {
    Paragraph(std::string& text, int& scroll_offset)
        : text(text), scroll_offset(scroll_offset) {}

    std::string& text;
    int&         scroll_offset;

    style::Style   text_style{style::Style::None};
    ComponentFlags flags{};
    style::Color   border_color{255, 255, 255, 255};

    mutable int                      last_height{0};
    mutable std::vector<std::string> m_lines;

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto [w, h] = content.size();
        if(w <= 0 || h <= 0) return;
        last_height = h;

        m_lines        = wrap(text, w);
        int max_offset = std::max(0, static_cast<int>(m_lines.size()) - h);
        scroll_offset  = std::clamp(scroll_offset, 0, max_offset);

        auto   pos   = content.position();
        size_t begin = static_cast<size_t>(scroll_offset);
        size_t end   = std::min(m_lines.size(), begin + static_cast<size_t>(h));
        for(size_t i = begin; i < end; i++) {
            cmd.push(renderer::DrawText{
                .position = pos,
                .text     = m_lines[i],
                .style    = text_style,
            });
            pos.y++;
        }
    }

    [[nodiscard]]
    bool handle(const input::Event& event) {
        switch(event.code) {
            case input::Key::Up:
                scroll_offset = std::max(0, scroll_offset - 1);
                return true;
            case input::Key::Down:
                scroll_offset++;
                return true;
            default:
                return false;
        }
    }

private:
    [[nodiscard]]
    static std::vector<std::string> wrap(const std::string& text, int width) {
        std::vector<std::string> lines;
        std::istringstream       words(text);
        std::string              word, line;
        while(words >> word) {
            if(line.empty()) line = word;
            else if(line.size() + 1 + word.size() <= static_cast<size_t>(width))
                line += " " + word;
            else {
                lines.push_back(line);
                line = word;
            }
        }
        if(!line.empty()) lines.push_back(line);
        return lines;
    }
};

}  // namespace trux::component
