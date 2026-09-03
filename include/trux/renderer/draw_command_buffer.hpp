/// @file draw_command_buffer.hpp
/// @brief DrawCommandBuffer: an append-only list of DrawCommands
///        recorded during a build pass.

#pragma once

#include "trux/renderer/draw_command.hpp"

#include <vector>

namespace trux::renderer {

/// Ordered, append-only collection of DrawCommand%s.
///
/// Components receive a reference to one of these in their `build()`
/// method and call push() to record what they want drawn; they never
/// draw directly. Renderer owns the buffer used during a frame and
/// replays it via Renderer::resolve() (see the private `resolve()`
/// implementation) after the whole tree has built.
class DrawCommandBuffer {
public:
    /// Appends `command` to the end of the buffer.
    void push(DrawCommand command) { m_commands.push_back(std::move(command)); }

    /// Removes all recorded commands. Called at the start of each
    /// frame by Renderer::begin_draw().
    void clear() { m_commands.clear(); }

    /// The commands recorded so far, in push order.
    [[nodiscard]]
    const std::vector<DrawCommand>& commands() const noexcept {
        return m_commands;
    }

private:
    std::vector<DrawCommand> m_commands;
};
}  // namespace trux::renderer
