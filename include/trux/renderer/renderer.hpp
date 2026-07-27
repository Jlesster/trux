#pragma once

namespace trux::renderer {
class Renderer {
public:
    void draw();

    void begin_draw();
    void end_draw();
};
}  // namespace trux::renderer
