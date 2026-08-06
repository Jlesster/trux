#pragma once

#include "trux/component/component.hpp"
#include "trux/focus/focus_manager.hpp"
#include "trux/input/event.hpp"
#include "trux/layout/layout.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell_buffer.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <functional>
#include <optional>
#include <string_view>
#include <vector>

namespace trux::renderer {

struct RenderBatch {
    layout::Position  position;
    std::vector<Cell> cells;
};

using DrawCommands  = std::vector<DrawCommand>;
using RenderBatches = std::vector<RenderBatch>;

class Renderer {
public:
    explicit Renderer(layout::Size);

    void clear();

    void begin_draw();
    void end_draw();
    void commit();

    void resize(layout::Size size);
    void resize(layout::Region& root);

    [[nodiscard]]
    CellBuffer& back_buffer() noexcept;
    [[nodiscard]]
    const CellBuffer& back_buffer() const noexcept;
    [[nodiscard]]
    const CellBuffer& front_buffer() const noexcept;
    [[nodiscard]]
    RenderBatches build_batches() const;
    [[nodiscard]]
    static bool can_merge(const Cell& prev, const Cell& next);

    void put(layout::Position, Cell);
    void put(layout::Position, char32_t);

    template <component::ComponentType T>
    void push(T& component, layout::Region);
    void push(component::ComponentBase& component, layout::Region area);

    void text(layout::Region, layout::Position, std::string_view);

    void set_clip(layout::Region);
    void clear_clip();

    [[nodiscard]]
    bool dispatch(const input::Event&);
    [[nodiscard]]
    focus::FocusManager focus() noexcept {
        return m_focus;
    }
    [[nodiscard]]
    layout::Region region() const noexcept {
        return m_root;
    }

    inline const RenderBatches& batches() const noexcept { return m_batches; }

private:
    struct Handler {
        focus::FocusID id;
        layout::Region region;

        std::function<bool(const input::Event&)> fn;
    };

    CellBuffer        m_front;
    CellBuffer        m_back;
    RenderBatches     m_batches;
    DrawCommandBuffer m_commands;

    std::optional<layout::Region> m_clip;

    layout::Size   m_size;
    layout::Region m_root{
        {0, 0},
        {0, 0}
    };

    focus::FocusManager  m_focus;
    std::vector<Handler> m_handlers;

    void resolve();
};

template <component::ComponentType T>
void Renderer::push(T& component, layout::Region area) {
    component.build(area, m_commands);
    if constexpr(input::Handleable<T>) {
        auto id = static_cast<focus::FocusID>(&component);
        m_focus.register_focusable(id);
        m_handlers.push_back(
            Handler{id, area, [&component](const input::Event& e) {
                        return component.handle(e);
                    }});
    }
}

}  // namespace trux::renderer
