/// @file label.hpp
/// @brief Label: the simplest component, drawing a single line of
///        static text.

#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace trux::component {
/// A single line of non-interactive text, drawn at the top-left of
/// its area. Not input::Handleable. Owns its `text`, so it can be a
/// default-constructed struct member and assigned later
/// (`ui.title.text = "hi"`) without lifetime concerns.
struct Label {
    Label() = default;

    /// Constructs a label showing `text` (copied).
    explicit Label(std::string_view text)
        : text(text) {}

    std::string text;

    ComponentFlags flags{};
    /// Foreground color; unset keeps the default (see component::Color).
    std::optional<style::Color> text_color;

    /// Draws `text` starting at `area`'s top-left corner.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        renderer::DrawText t{{area.position().x, area.position().y}, text};
        if(text_color) t.fg = *text_color;
        cmd.push(t);
    }
};
}  // namespace trux::component
