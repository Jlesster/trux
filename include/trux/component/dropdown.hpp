#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/input/event.hpp"
#include "trux/input/key.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

#include <algorithm>
#include <functional>
#include <string_view>

namespace trux::component {
template <typename R, typename Proj = std::identity> struct Dropdown {
    explicit Dropdown(R& options, int& scroll_offset, Proj proj = {})
        : options(options), scroll_offset(scroll_offset), proj(proj) {}

    R&   options;
    Proj proj;

    size_t selected{0};
    bool   open{false};

    style::Style   highlight_style{style::Style::Reverse};
    ComponentFlags flags{};
    style::Color   border_color{255, 255, 255, 255};

    int&        scroll_offset;
    mutable int last_height{0};

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            content = inset(area);
        }

        auto pos = content.position();
        if(!open) {
            cmd.push(renderer::DrawText{
                .position = pos,
                .text = options.empty() ? std::string_view{"--"}
                                        : std::invoke(proj, options[selected]),
            });
            return;
        }
        auto [w, h] = content.size();
        last_height = h;

        int max_offset = std::max(0, static_cast<int>(options.size()) - h);
        scroll_offset  = std::clamp(scroll_offset, 0, max_offset);

        size_t begin = static_cast<size_t>(scroll_offset);
        size_t end   = std::min(options.size(),
                                begin + static_cast<size_t>(std::max(h, 0)));
        for(size_t i = begin; i < end; i++) {
            cmd.push(renderer::DrawText{
                .position = pos,
                .text     = std::invoke(proj, options[i]),
                .style = (static_cast<int>(i) == selected) ? highlight_style
                                                           : style::Style::None,
            });
            pos.y++;
        }
    }

    [[nodiscard]]
    bool handle(const input::Event& event) {
        if(options.empty()) return false;

        if(!open) {
            switch(event.code) {
                case input::Key::Enter:
                case static_cast<char32_t>(' '):
                    open = true;
                    return true;
                default:
                    return false;
            }
        }

        switch(event.code) {
            case input::Key::Up:
            case static_cast<char32_t>('k'):
                selected = (selected == 0)
                               ? static_cast<int>(options.size()) - 1
                               : selected - 1;
                return true;

            case input::Key::Down:
            case static_cast<char32_t>('j'):
                selected = (selected == static_cast<int>(options.size()) - 1)
                               ? 0
                               : selected + 1;
                return true;

            case input::Key::Enter:
            case static_cast<char32_t>(' '):
                open = false;
                return true;

            case input::Key::Escape:
                open = false;
                return true;

            default:
                return true;
        }
    }
};
}  // namespace trux::component
