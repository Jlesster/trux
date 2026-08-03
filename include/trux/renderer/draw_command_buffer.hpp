#pragma once

#include "trux/renderer/draw_command.hpp"

#include <vector>

namespace trux::renderer {

class DrawCommandBuffer {
public:
    void push(DrawCommand command) { m_commands.push_back(std::move(command)); }

    void clear() { m_commands.clear(); }

    [[nodiscard]]
    const std::vector<DrawCommand>& commands() const noexcept {
        return m_commands;
    }

private:
    std::vector<DrawCommand> m_commands;
};
}  // namespace trux::renderer
