#pragma once

#include "trux/component/component.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <string_view>
#include <vector>

namespace trux::component {
struct Dropdown {
    explicit Dropdown(std::vector<std::string>& options)
        : options(to_views(options)) {}

    std::vector<std::string_view> options;

    size_t selected{};

    bool open{false};

    ComponentFlags flags{};

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto pos = area.position();
        for(auto item : options) {
            cmd.push(renderer::DrawText{
                .position = pos,
                .text     = item,
            });
            pos.y++;
        }
    }
};
}  // namespace trux::component
