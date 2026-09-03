/// @file label.hpp
/// @brief Label: the simplest component, drawing a single line of
///        static text.

#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <string_view>

namespace trux::component {
/// A single line of non-interactive text, drawn at the top-left of
/// its area. Not input::Handleable.
struct Label {
    /// Constructs a label showing `text`.
    explicit Label(std::string_view text) : text(text) {}

    std::string_view text = "";

    ComponentFlags flags{};

    /// Draws `text` starting at `area`'s top-left corner.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        cmd.push(renderer::DrawText{
            {area.position().x, area.position().y},
            text
        });
    }
};
}  // namespace trux::component
