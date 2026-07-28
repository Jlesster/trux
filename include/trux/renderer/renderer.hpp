#pragma once

#include "trux/layout/position.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell_buffer.hpp"

#include <optional>
#include <string_view>

namespace trux::renderer {

class Renderer {
public:
    explicit Renderer(layout::Size);

    void clear();

    void begin_draw();
    void end_draw();

    void resize(layout::Size size);

    [[nodiscard]]
    CellBuffer& back_buffer() noexcept;
    [[nodiscard]]
    const CellBuffer& back_buffer() const noexcept;

    void put(layout::Position, Cell);
    void put(layout::Position, char32_t);

    void text(layout::Region, layout::Position, std::string_view);

    void set_clip(layout::Region);
    void clear_clip();

private:
    CellBuffer m_front;
    CellBuffer m_back;

    std::optional<layout::Region> m_clip;

    layout::Size m_size;
};
}  // namespace trux::renderer
