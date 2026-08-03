#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <string_view>

namespace trux::component {
struct Label {
    explicit Label(std::string_view text) : text(text) {}

    std::string_view text = "";

    ComponentFlags flags{};

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        cmd.push(renderer::DrawText{area.position(), text});
    }
};
}  // namespace trux::component
