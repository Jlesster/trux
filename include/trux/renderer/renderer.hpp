/// @file renderer.hpp
/// @brief Renderer: the double-buffered frame driver that turns a
///        component tree into terminal output and routes input.

#pragma once

#include "trux/component/component.hpp"
#include "trux/diag/boundary.hpp"
#include "trux/diag/compile.hpp"
#include "trux/diag/diag.hpp"
#include "trux/focus/focus_manager.hpp"
#include "trux/input/event.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell_buffer.hpp"
#include "trux/renderer/draw_command.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

#include <chrono>
#include <concepts>
#include <functional>
#include <optional>
#include <string_view>
#include <typeinfo>
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

using DrawCommands = std::vector<DrawCommand>;
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
    void push(component::ComponentBase& component, layout::Region area,
              bool modal = false);

    /// Builds both children of `split` into `area` and recursively
    /// registers any components nested within them for input
    /// dispatch (see register_region()).
    void push(layout::Split& split, layout::Region area, bool modal = false);

    /// Catches push() of something that is not a component, so the error
    /// says what is missing instead of listing every overload.
    template <typename T>
        requires(!component::ComponentType<T> &&
                 !std::derived_from<T, component::ComponentBase> &&
                 !std::derived_from<T, layout::Split>)
    void push(T&, layout::Region, bool = false) {
        TRUX_CHECK("T0004",
                   "this type can't be pushed to the Renderer",
                   "it has no `flags` member of type component::ComponentFlags",
                   "add `component::ComponentFlags flags{};` to the type",
                   component::has_flags_member<T>);
        TRUX_CHECK(
            "T0005",
            "this type can't be pushed to the Renderer",
            "it has no `void build(Region, DrawCommandBuffer&) const`" TRUX_NL
            "(check it is const and returns void)",
            "add `void build(layout::Region area," TRUX_NL
            "renderer::DrawCommandBuffer& cmd) const` to the type",
            component::has_build_member<T>);
    }

    /// Generic overload for any type satisfying
    /// component::ComponentType, avoiding the need to type-erase
    /// into component::ComponentBase.
    ///
    /// If `T` has a `border_color` member (see
    /// component::HasBorderColor), it is set to the focused or
    /// unfocused border color (see set_border_colors()) before
    /// building, based on whether `component` currently has focus.
    /// If `T` also has `border_color_fixed`/`border_color_active`
    /// (see component::BorderColor), those override the defaults;
    /// `border_color_active` applies only while focused.
    /// If `T` is not input::Handleable, it is built but never
    /// registered as an input handler. A handleable `T` that exposes a
    /// `focusable` member set to false is likewise built (and may still
    /// handle events if modal) but is skipped for Tab traversal and
    /// mouse click-to-focus.
    ///
    /// @tparam T Component type satisfying component::ComponentType.
    template <component::ComponentType T>
    void push(T& component, layout::Region area, bool modal = false) {
        auto id = static_cast<focus::FocusID>(&component);
        const bool handleable = input::Handleable<T>;
        bool focusable = handleable;
        if constexpr(requires { static_cast<bool>(component.focusable); })
            focusable = handleable && static_cast<bool>(component.focusable);
        push_generic(
            id,
            area,
            modal,
            handleable,
            focusable,
            typeid(T),
            [&] {
                if constexpr(requires { component.title.active; })
                    component.title.active = m_focus.is_focused(id);
                if constexpr(component::HasBorderColor<T>) {
                    const bool focused = m_focus.is_focused(id);
                    component.border_color =
                        focused ? m_focus_color : m_default_color;
                    if constexpr(requires { component.border_color_fixed; })
                        if(component.border_color_fixed)
                            component.border_color =
                                *component.border_color_fixed;
                    if constexpr(requires { component.border_color_active; })
                        if(focused && component.border_color_active)
                            component.border_color =
                                *component.border_color_active;
                }
                if constexpr(requires { component.prepare(area); })
                    component.prepare(area);
                if constexpr(requires {
                                 {
                                     component.frame_interval_ms()
                                 } -> std::convertible_to<int>;
                             })
                    request_tick(std::chrono::milliseconds(
                        component.frame_interval_ms()));
                component.build(area, m_commands);
            },
            [&component](const input::Event& e) -> bool {
                if constexpr(input::Handleable<T>) return component.handle(e);
                else return false;
            },
            [&component]() -> bool {
                if constexpr(requires {
                                 {
                                     component.captures_tab()
                                 } -> std::same_as<bool>;
                             })
                    return component.captures_tab();
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
        m_focus_color = focused;
        m_default_color = unfocused;
    }

    /// Records a DrawText command placing `text` at `pos`, local to
    /// `region`'s absolute position on screen.
    void text(layout::Region, layout::Position, std::string_view);

    /// Records a SetClip command, restricting subsequent draws to
    /// `region` (intersected with any currently active clip).
    void set_clip(layout::Region);

    /// Pops the most recent set_clip(), restoring the clip (and modal
    /// flag) that was active before it. Unbalanced calls are logged and
    /// ignored.
    void clear_clip();

    /// Drops every active clip. Called between draw layers so one
    /// layer's unbalanced SetClip can't leak into the next.
    void reset_clips();

    /// Routes an input event to the appropriate handler and returns
    /// whether it was consumed.
    ///
    /// Resize events are handled directly (see resize(layout::Size)).
    /// Otherwise: any active modal handler takes priority; Tab/Shift+Tab
    /// advance focus via the FocusManager, unless the focused component
    /// opts in with `captures_tab()` and consumes the key itself (for
    /// example an editor indenting); a mouse press first moves
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

    /// Whether `component` is the one that currently has input focus.
    ///
    /// Takes the same object that was passed to push(), so callers
    /// need no casting or FocusID handling. Reflects the focus state
    /// as of the most recent end_draw().
    ///
    /// @tparam T         Type of the component being queried.
    /// @param component  The component to test, by identity (address).
    /// @return true if `component` currently has focus, false if it is
    ///         unfocused or was not pushed this frame.
    template <typename T>
    [[nodiscard]]
    bool is_focused(const T& component) const noexcept {
        return m_focus.is_focused(static_cast<focus::FocusID>(&component));
    }

    /// Returns the focused component as a `T*`.
    ///
    /// Only components pushed directly (e.g. `push(menu, area)`) can be
    /// recovered this way. Components reached through a type-erased
    /// component::ComponentBase or a layout::Split are not matched and
    /// yield nullptr. The pointer is valid until the next begin_draw().
    ///
    /// @code{.cpp}
    /// if(auto* menu = renderer.focused_as<Menu>())
    ///     selected = menu->selection;
    /// @endcode
    ///
    /// @tparam T  Exact concrete type the component was pushed as.
    ///            Must not derive from component::ComponentBase.
    /// @return Pointer to the focused component if it is a `T`;
    ///         nullptr if nothing is focused or the focused component
    ///         is of a different type.
    template <typename T>
        requires(!std::derived_from<T, component::ComponentBase>)
    [[nodiscard]]
    T* focused_as() const noexcept {
        const focus::FocusID id = m_focus.focused();
        if(!id) return nullptr;
        for(const Handler& h : m_handlers) {
            if(h.id != id) continue;
            if(!h.type || *h.type != typeid(T)) return nullptr;
            return static_cast<T*>(const_cast<void*>(id));
        }
        return nullptr;
    }

    /// The Region the focused component was pushed into during the
    /// most recent frame.
    ///
    /// @return The focused component's Region, or std::nullopt if
    ///         nothing is focused.
    [[nodiscard]]
    std::optional<layout::Region> focused_region() const {
        const focus::FocusID id = m_focus.focused();
        if(!id) return std::nullopt;
        for(const Handler& h : m_handlers)
            if(h.id == id) return h.region;
        return std::nullopt;
    }

    /// Snapshot of the panel that currently has input focus.
    ///
    /// A lightweight, copyable handle returned by active_panel(). It
    /// does not own the component; it is valid until the next
    /// begin_draw(). Test it with `if(panel)` before use.
    struct Panel {
        /// Identity of the component (address it was pushed as).
        focus::FocusID id{nullptr};
        /// Dynamic type the component was pushed as, if known.
        const std::type_info* type{nullptr};
        /// Region the component occupied in the most recent frame.
        layout::Region region{};

        /// @return true if a panel is focused, false for the empty panel.
        [[nodiscard]]
        explicit operator bool() const noexcept {
            return id != nullptr;
        }

        /// Whether the focused panel was pushed directly as a `T`.
        ///
        /// @tparam T  Concrete component type (not a ComponentBase).
        template <typename T>
            requires(!std::derived_from<T, component::ComponentBase>)
        [[nodiscard]]
        bool is() const noexcept {
            return id && type && *type == typeid(T);
        }

        /// The panel as a `T*`.
        ///
        /// @tparam T  Concrete component type (not a ComponentBase).
        /// @return Pointer to the component, or nullptr if the panel is
        ///         empty or is not a `T`.
        template <typename T>
            requires(!std::derived_from<T, component::ComponentBase>)
        [[nodiscard]]
        T* as() const noexcept {
            return is<T>() ? static_cast<T*>(const_cast<void*>(id)) : nullptr;
        }
    };

    /// The panel that currently has input focus.
    ///
    /// Never fails: when nothing is focused the returned Panel is empty
    /// (`!panel`). The reference points at renderer-owned storage that
    /// is refreshed on every call, so it always reflects the current
    /// focus; copy it (`auto p = ...`) if you need a stable snapshot.
    /// Does not alter focus().focused(), which is unchanged.
    ///
    /// @code{.cpp}
    /// auto& panel = renderer.active_panel();   // live view
    /// auto  snap  = renderer.active_panel();   // copy
    /// auto* menu  = renderer.active_panel().as<Menu>();
    /// if(panel) draw_status(panel.region);
    /// @endcode
    ///
    /// @return Reference to the current active Panel.
    [[nodiscard]]
    const Panel& active_panel() const {
        m_active_panel = {};
        const focus::FocusID id = m_focus.focused();
        if(!id) return m_active_panel;
        for(const Handler& h : m_handlers) {
            if(h.id != id) continue;
            m_active_panel = Panel{h.id, h.type, h.region};
            break;
        }
        return m_active_panel;
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

    /// How often the app wants to be woken to redraw, as asked for by
    /// the components pushed in the most recent frame (smallest
    /// non-zero request wins). Zero means nothing asked for ticks.
    [[nodiscard]]
    std::chrono::milliseconds tick_interval() const noexcept {
        return m_tick;
    }

    /// Asks for a Tick at least every `interval` until the next
    /// begin_draw(). Non-positive requests are ignored.
    void request_tick(std::chrono::milliseconds interval) noexcept {
        if(interval.count() <= 0) return;
        if(m_tick.count() == 0 || interval < m_tick) m_tick = interval;
    }

    /// Shows the newest warning/error (diag::warn()/diag::error()) as a
    /// one-line strip on the bottom row for a few seconds. On by
    /// default; the full history is in the log (see diag::Log::path()).
    void set_diagnostics_overlay(bool on) noexcept { m_diag_overlay = on; }

private:
    /// One registered input target: the region it covers, whether it
    /// belongs to a modal layer, and the callback to invoke when it
    /// receives an event (see push(), register_region(), dispatch()).
    struct Handler {
        focus::FocusID id;
        layout::Region region;
        bool modal{false};

        std::function<bool(const input::Event&)> fn;
        /// True if the component wants Tab offered to fn first.
        std::function<bool()> captures_tab;
        /// Dynamic type, for diagnostics only.
        const std::type_info* type{nullptr};
    };

    CellBuffer m_front;
    CellBuffer m_back;
    RenderBatches m_batches;
    DrawCommandBuffer m_commands;
    std::vector<bool> m_owner;
    bool m_current_modal{false};

    /// Saved state of one active SetClip: what was in effect before it.
    struct ClipFrame {
        std::optional<layout::Region> clip;
        bool modal;
    };

    std::optional<layout::Region> m_clip;
    std::vector<ClipFrame> m_clip_stack;

    layout::Size m_size;
    std::chrono::milliseconds m_tick{0};
    layout::Region m_root{{0, 0}, {0, 0}};

    focus::FocusManager m_focus;
    std::vector<Handler> m_handlers;
    mutable Panel m_active_panel;

    style::Color m_focus_color{style::Color::ActiveBorderColor()};
    style::Color m_default_color{255, 255, 255, 255};

    bool m_diag_overlay{true};

    /// Draws the diagnostics strip (see set_diagnostics_overlay()).
    void push_diagnostic_toast();
    /// Runs a component's build; if it throws, discards what it
    /// recorded, logs it, and draws an error box in `area` instead.
    template <typename Fn>
    void guarded_build(const std::type_info& type, layout::Region area,
                       Fn&& fn) {
        const auto cp = m_commands.checkpoint();
        try {
            fn();
        } catch(...) { build_failed(type, area, cp); }
    }
    /// Must be called from inside a catch block.
    void build_failed(const std::type_info& type, layout::Region area,
                      DrawCommandBuffer::Checkpoint cp);
    /// Invokes a handler, turning an exception into a logged "not consumed".
    bool call_handler(const Handler& handler, const input::Event& e);

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
    /// the SetClip/build/ClearClip sequence, registers `id` for Tab
    /// traversal when `focusable`, and registers an input Handler when
    /// `handleable` and reachable (modal, or focusable).
    template <typename BuildFn, typename HandleFn>
    void push_generic(focus::FocusID id, layout::Region area, bool modal,
                      bool handleable, bool focusable,
                      const std::type_info& type, BuildFn&& build_fn,
                      HandleFn&& handle_fn,
                      std::function<bool()> captures_tab = {}) {
        if(!modal && focusable) m_focus.register_focusable(id, area.rect());

        m_commands.push(SetClip{area, modal});
        guarded_build(type, area, [&] { build_fn(); });
        m_commands.push(ClearClip{});

        if(handleable && (modal || focusable))
            m_handlers.push_back(Handler{id,
                                         area,
                                         modal,
                                         std::forward<HandleFn>(handle_fn),
                                         std::move(captures_tab),
                                         &type});
    }
};

}  // namespace trux::renderer
