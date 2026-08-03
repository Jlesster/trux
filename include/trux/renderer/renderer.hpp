#pragma once

#include "trux/component/component.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell_buffer.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <optional>
#include <string_view>

namespace trux::renderer {

using DrawCommands = std::vector<DrawCommand>;

class Renderer {
public:
    explicit Renderer(layout::Size);

    void clear();

    void begin_draw();
    void end_draw();
    void commit();

    void resize(layout::Size size);

    [[nodiscard]]
    CellBuffer& back_buffer() noexcept;
    [[nodiscard]]
    const CellBuffer& back_buffer() const noexcept;
    [[nodiscard]]
    const CellBuffer& front_buffer() const noexcept;

    void put(layout::Position, Cell);
    void put(layout::Position, char32_t);

    template <component::ComponentType T>
    void push(const T& component, layout::Region);

    void text(layout::Region, layout::Position, std::string_view);

    void set_clip(layout::Region);
    void clear_clip();

private:
    CellBuffer m_front;
    CellBuffer m_back;

    DrawCommandBuffer m_commands;

    std::optional<layout::Region> m_clip;

    layout::Size m_size;

    void resolve();
};

template <component::ComponentType T>
void Renderer::push(const T& component, layout::Region area) {
    component.build(area, m_commands);
}

}  // namespace trux::renderer
