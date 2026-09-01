#pragma once

#include "trux/component/component.hpp"
#include "trux/focus/focus_manager.hpp"
#include "trux/input/event.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell_buffer.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

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

    void push(component::ComponentBase& component,
              layout::Region            area,
              bool                      modal = false);

    template <component::ComponentType T>
    void push(T& component, layout::Region area, bool modal = false) {
        auto id = static_cast<focus::FocusID>(&component);
        push_generic(
            id,
            area,
            modal,
            input::Handleable<T>,
            [&] {
                if constexpr(component::HasBorderColor<T>) {
                    component.border_color = m_focus.is_focused(id)
                                                 ? m_focus_color
                                                 : m_default_color;
                }
                component.build(area, m_commands);
            },
            [&component](const input::Event& e) -> bool {
                if constexpr(input::Handleable<T>) return component.handle(e);
                else return false;
            });
    }

    void set_border_colors(style::Color focused,
                           style::Color unfocused = {255, 255, 255, 255}) {
        m_focus_color   = focused;
        m_default_color = unfocused;
    }

    void text(layout::Region, layout::Position, std::string_view);
    void set_clip(layout::Region);
    void clear_clip();

    [[nodiscard]]
    bool dispatch(const input::Event&);
    [[nodiscard]]
    const focus::FocusManager& focus() noexcept {
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
        bool           modal{false};

        std::function<bool(const input::Event&)> fn;
    };

    CellBuffer        m_front;
    CellBuffer        m_back;
    RenderBatches     m_batches;
    DrawCommandBuffer m_commands;
    std::vector<bool> m_owner;
    bool              m_current_modal{false};

    std::optional<layout::Region> m_clip;

    layout::Size   m_size;
    layout::Region m_root{
        {0, 0},
        {0, 0}
    };

    focus::FocusManager  m_focus;
    std::vector<Handler> m_handlers;

    style::Color m_focus_color{style::Color::ActiveBorderColor()};
    style::Color m_default_color{255, 255, 255, 255};

    void resolve();
    void put_opaque(layout::Position, Cell);
    void coalesce_borders();

    template <typename BuildFn, typename HandleFn>
    void push_generic(focus::FocusID id,
                      layout::Region area,
                      bool           modal,
                      bool           handleable,
                      BuildFn&&      build_fn,
                      HandleFn&&     handle_fn) {
        if(!modal && handleable) m_focus.register_focusable(id);

        m_commands.push(SetClip{area, modal});
        build_fn();
        m_commands.push(ClearClip{});

        if(handleable)
            m_handlers.push_back(
                Handler{id, area, modal, std::forward<HandleFn>(handle_fn)});
    }
};

}  // namespace trux::renderer
