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
    /// Appends `command` to the end of the buffer, or of the overlay
    /// layer while set_overlay(true) is in effect (see OverlayScope).
    void push(DrawCommand command) {
        (m_overlay ? m_overlay_commands : m_commands)
            .push_back(std::move(command));
    }

    /// Routes subsequent push() calls to the overlay layer, which the
    /// Renderer replays after every normal command so it paints on top
    /// of the whole frame. Prefer OverlayScope over calling this directly.
    void set_overlay(bool on) noexcept { m_overlay = on; }
    [[nodiscard]] bool overlay() const noexcept { return m_overlay; }

    /// Removes all recorded commands. Called at the start of each
    /// frame by Renderer::begin_draw().
    void clear() {
        m_commands.clear();
        m_overlay_commands.clear();
        m_overlay = false;
    }

    /// The overlay commands recorded so far, in push order.
    [[nodiscard]]
    const std::vector<DrawCommand>& overlay_commands() const noexcept {
        return m_overlay_commands;
    }

    /// The commands recorded so far, in push order.
    [[nodiscard]]
    const std::vector<DrawCommand>& commands() const noexcept {
        return m_commands;
    }

private:
    std::vector<DrawCommand> m_commands;
    std::vector<DrawCommand> m_overlay_commands;
    bool m_overlay{false};
};

/// While alive, everything pushed to `cmd` lands on the overlay layer,
/// clipped to `region` and drawn over an opaquely cleared background, so
/// it covers whatever the rest of the frame drew there regardless of
/// build order. The clip is tagged as its own border-owner (see
/// SetClip::modal), so overlay borders never fuse with borders beneath.
class OverlayScope {
public:
    OverlayScope(DrawCommandBuffer& cmd, layout::Region region)
        : m_cmd(cmd),
          m_prev(cmd.overlay()) {
        m_cmd.set_overlay(true);
        m_cmd.push(SetClip{region, true});
        m_cmd.push(Fill{region, Cell{}});
    }
    ~OverlayScope() {
        m_cmd.push(ClearClip{});
        m_cmd.set_overlay(m_prev);
    }
    OverlayScope(const OverlayScope&) = delete;
    OverlayScope& operator=(const OverlayScope&) = delete;

private:
    DrawCommandBuffer& m_cmd;
    bool m_prev;
};

}  // namespace trux::renderer
