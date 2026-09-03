/// @file renderer.hpp
/// @brief Renderer: the double-buffered frame driver that turns a
///        component tree into terminal output and routes input.

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

/// A contiguous run of same-styled cells on one row, as produced by
/// Renderer::build_batches(). Terminal backends typically turn each
/// batch into a single cursor-move + styled-write, which is why
/// adjacent, style-compatible diffed cells are coalesced into
/// batches rather than emitted individually (see can_merge()).
struct RenderBatch {
    /// Position of the first cell in the run.
    layout::Position position;
    /// The run's cells, left to right.
    std::vector<Cell> cells;
};

using DrawCommands  = std::vector<DrawCommand>;
using RenderBatches = std::vector<RenderBatch>;

/// Drives one frame of rendering: collects DrawCommands from a
/// component tree into a back buffer, diffs it against the last
/// committed (front) buffer to find what changed, and also owns
/// focus tracking and input dispatch for the tree it just built.
///
/// Typical per-frame usage:
/// @code{.cpp}
/// renderer.begin_draw();
/// renderer.push(root_component, renderer.region());
/// renderer.end_draw();
/// // send renderer.batches() to the terminal backend, then:
/// renderer.commit();
/// @endcode
///
/// Renderer is not itself thread-safe and is intended to be driven
/// from a single UI/event loop thread.
class Renderer {
public:
    /// Constructs a renderer with both buffers sized to `size`
    /// (typically the initial terminal size).
    explicit Renderer(layout::Size);

    /// Resets the back buffer to its default (empty) state without
    /// affecting the front buffer or committed frame.
    void clear();

    /// Starts a new frame: clears the back buffer, drops last
    /// frame's draw commands/input handlers, resets focus tracking
    /// for re-registration, and rebuilds the root Region from the
    /// current size. Call once at the start of each frame, before
    /// any push() calls.
    void begin_draw();

    /// Ends the current frame: replays recorded draw commands into
    /// the back buffer, merges adjacent border glyphs so they join
    /// visually across components (see component::classify_border_glyph()),
    /// and rebuilds batches() by diffing against the front buffer.
    /// Call once after all push() calls for the frame.
    void end_draw();

    /// Copies the back buffer into the front buffer, marking the
    /// current frame as displayed. Call after handing batches() to
    /// the terminal backend for output.
    void commit();

    /// Resizes both buffers to `size` if it differs from the current
    /// size, invalidating the front buffer so the next end_draw()
    /// reports every cell as changed (forcing a full repaint). A
    /// no-op if `size` matches the current size.
    void resize(layout::Size size);

    /// Recursively resizes `root`'s split tree to match the
    /// renderer's current size (see layout::propagate_resize()).
    /// Call after resize(layout::Size) if you're maintaining your
    /// own persistent Region tree rather than rebuilding it each
    /// frame via begin_draw().
    void resize(layout::Region& root);

    /// The buffer currently being drawn into (this frame's, not yet
    /// committed).
    [[nodiscard]]
    CellBuffer& back_buffer() noexcept;
    /// @copydoc back_buffer()
    [[nodiscard]]
    const CellBuffer& back_buffer() const noexcept;

    /// The buffer representing the last committed (displayed) frame.
    [[nodiscard]]
    const CellBuffer& front_buffer() const noexcept;

    /// Diffs the back buffer against the front buffer and groups the
    /// changed cells into row runs of mergeable style (see
    /// can_merge()). Called internally by end_draw(); exposed for
    /// callers that want to recompute batches without a full frame.
    [[nodiscard]]
    RenderBatches build_batches() const;

    /// Whether two adjacent cells can be emitted as part of the same
    /// RenderBatch — true when they share foreground, background,
    /// and style (glyph is allowed to differ within a batch).
    [[nodiscard]]
    static bool can_merge(const Cell& prev, const Cell& next);

    /// Alpha-blends `cell` onto the back buffer at `pos` (see
    /// renderer::blend()). No-op if `pos` is outside the current
    /// clip region or buffer bounds.
    void put(layout::Position, Cell);

    /// Convenience overload: blends a cell holding only the given
    /// glyph (default color/style) at `pos`.
    void put(layout::Position, char32_t);

    /// Builds `component` into `area`: records its draw commands,
    /// and — unless `modal` is true — registers it as focusable and
    /// as an input handler for events landing within `area`.
    ///
    /// @param component Component to build. Must outlive this frame.
    /// @param area      Region to build the component into.
    /// @param modal      Marks the pushed content as belonging to a
    ///                   modal layer: it is excluded from normal
    ///                   Tab-based focus traversal but takes priority
    ///                   during dispatch() (see dispatch()).
    void push(component::ComponentBase& component,
              layout::Region            area,
              bool                      modal = false);

    /// Builds both children of `split` into `area` and recursively
    /// registers any components nested within them for input
    /// dispatch (see register_region()).
    void push(layout::Split& split, layout::Region area, bool modal = false);

    /// Generic overload for any type satisfying
    /// component::ComponentType, avoiding the need to type-erase
    /// into component::ComponentBase.
    ///
    /// If `T` has a `border_color` member (see
    /// component::HasBorderColor), it is set to the focused or
    /// unfocused border color (see set_border_colors()) before
    /// building, based on whether `component` currently has focus.
    /// If `T` is not input::Handleable, it is built but never
    /// registered as an input handler.
    ///
    /// @tparam T Component type satisfying component::ComponentType.
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

    /// Sets the border colors used to distinguish a focused
    /// component from an unfocused one (applied automatically by the
    /// templated push() for components with a `border_color` member).
    ///
    /// @param focused   Color to use for the currently focused component.
    /// @param unfocused Color to use for all other components. Defaults
    ///                  to opaque white.
    void set_border_colors(style::Color focused,
                           style::Color unfocused = {255, 255, 255, 255}) {
        m_focus_color   = focused;
        m_default_color = unfocused;
    }

    /// Records a DrawText command placing `text` at `pos`, local to
    /// `region`'s absolute position on screen.
    void text(layout::Region, layout::Position, std::string_view);

    /// Records a SetClip command, restricting subsequent draws to
    /// `region` (intersected with any currently active clip).
    void set_clip(layout::Region);

    /// Records a ClearClip command, popping the most recent set_clip().
    void clear_clip();

    /// Routes an input event to the appropriate handler and returns
    /// whether it was consumed.
    ///
    /// Resize events are handled directly (see resize(layout::Size)).
    /// Otherwise: any active modal handler takes priority; Tab/Shift+Tab
    /// advance focus via the FocusManager; a mouse press first moves
    /// focus to the region under the cursor; and finally the event is
    /// delivered to whichever registered handler currently has focus.
    ///
    /// @return true if some handler consumed the event, false otherwise.
    [[nodiscard]]
    bool dispatch(const input::Event&);

    /// The focus manager tracking which pushed component currently
    /// has input focus.
    [[nodiscard]]
    const focus::FocusManager& focus() noexcept {
        return m_focus;
    }

    /// The root Region rebuilt by the most recent begin_draw(),
    /// spanning the renderer's full current size.
    [[nodiscard]]
    layout::Region region() const noexcept {
        return m_root;
    }

    /// The render batches computed by the most recent end_draw(),
    /// ready to be handed to a terminal backend for output.
    inline const RenderBatches& batches() const noexcept { return m_batches; }

private:
    /// One registered input target: the region it covers, whether it
    /// belongs to a modal layer, and the callback to invoke when it
    /// receives an event (see push(), register_region(), dispatch()).
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

    /// Replays every recorded DrawCommand into the back buffer.
    void resolve();
    /// Like put(), but overwrites the cell directly rather than
    /// alpha-blending; used for opaque fills.
    void put_opaque(layout::Position, Cell);
    /// Joins adjacent border glyphs belonging to the same owner so
    /// borders between neighboring components appear connected.
    void coalesce_borders();
    /// Recursively walks a (possibly split) region's component tree,
    /// registering every handleable component it finds as an input
    /// handler covering its own sub-region.
    void register_region(layout::Region, bool);

    /// Shared implementation behind every push() overload: records
    /// the SetClip/build/ClearClip sequence and, if `handleable`,
    /// registers an input Handler for `id`.
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
