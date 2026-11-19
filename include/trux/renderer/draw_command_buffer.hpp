/// @file draw_command_buffer.hpp
/// @brief DrawCommandBuffer: an append-only list of DrawCommands
///        recorded during a build pass.

#pragma once

#include "trux/renderer/draw_command.hpp"

#include <deque>
#include <string>
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
        m_scratch.clear();
        m_overlay = false;
    }

    /// A restore point for rollback(): lets the renderer discard what a
    /// component recorded before it threw, so a half-built component
    /// can't leave dangling SetClips or stray cells behind.
    struct Checkpoint {
        size_t commands{};
        size_t overlay_commands{};
        bool overlay{};
    };
    [[nodiscard]]
    Checkpoint checkpoint() const noexcept {
        return {m_commands.size(), m_overlay_commands.size(), m_overlay};
    }
    void rollback(const Checkpoint& cp) {
        if(m_commands.size() > cp.commands)
            m_commands.erase(m_commands.begin() + static_cast<ptrdiff_t>(cp.commands),
                             m_commands.end());
        if(m_overlay_commands.size() > cp.overlay_commands)
            m_overlay_commands.erase(
                m_overlay_commands.begin() + static_cast<ptrdiff_t>(cp.overlay_commands),
                m_overlay_commands.end());
        m_overlay = cp.overlay;
    }

    /// Stores `text` until the next clear() and returns a view that is
    /// valid for the rest of the frame. Use for DrawText content built
    /// on the fly (DrawText::text is not owned).
    [[nodiscard]]
    std::string_view own(std::string text) {
        return m_scratch.emplace_back(std::move(text));
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
    std::deque<std::string> m_scratch;  // deque: views stay valid on growth
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
