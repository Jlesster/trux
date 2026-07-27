#pragma once

#include "trux/layout/size.hpp"
#include "trux/renderer/cell_buffer.hpp"

namespace trux::renderer {
class Renderer {
public:
    explicit Renderer(layout::Size);

    void draw();

    void begin_draw();
    void end_draw();

private:
    CellBuffer m_front;
    CellBuffer m_back;
};
}  // namespace trux::renderer
