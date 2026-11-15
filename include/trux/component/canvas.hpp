/// @file canvas.hpp
/// @brief Canvas: a display-only pane that owns a grid of Cells and
///        blits it, for apps that rasterize their own content
///        (plots, 3D, braille graphics, ...).

#pragma once

#include "trux/component/border.hpp"
#include "trux/component/component_flags.hpp"
#include "trux/component/look.hpp"
#include "trux/layout/position.hpp"
#include "trux/layout/region.hpp"
#include "trux/layout/size.hpp"
#include "trux/renderer/cell.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace trux::component {

/// A pane that owns a row-major grid of renderer::Cell and draws it.
///
/// Canvas knows nothing about *what* is in the grid; the app writes
/// cells (directly via at()/set()/cells()) and trux just blits them.
/// It is display-only: there is no handle(), so it never becomes
/// focusable and never steals Tab or key events.
///
/// Typical per-frame use:
/// @code
/// canvas.fit(region);        // match the pane's content area
/// canvas.clear();
/// canvas.on_frame([](Canvas& c, float dt) { /* draw into c */ });
/// renderer.push(canvas, region);
/// @endcode
struct Canvas {
    /// Applies `look` (border, colours, title) to a new canvas.
    explicit Canvas(const Look& look) { apply_look(*this, look); }

    /// Border styling, e.g. `canvas | component::BorderRounded`.
    ComponentFlags flags{};
    /// Optional title drawn on the border (same behavior as Paragraph).
    Title title{};
    /// Written by Renderer::push() every frame; pin with
    /// `border_color_fixed` / `border_color_active`.
    style::Color border_color{255, 255, 255, 255};
    std::optional<style::Color> border_color_fixed;
    std::optional<style::Color> border_color_active;
    /// If set, the content area is first filled opaquely with this color.
    std::optional<style::Color> background;

    /// Whether prepare() blanks the grid before calling the on_frame() handler.
    bool auto_clear{true};

    /// Registers the per-frame handler. Renderer::push() calls it (via
    /// prepare()) after fitting/clearing the grid and before the cells
    /// are recorded, so whatever it writes is what gets drawn.
    ///
    /// The handler is called as `(Canvas& canvas, float dt, float elapsed,
    /// std::uint64_t frame)` and may take any leading subset of those:
    /// `(canvas)`, `(canvas, dt)`, `(canvas, dt, elapsed)` or all four.
    ///  - canvas:  already fitted to the pane (and cleared, if auto_clear);
    ///             use canvas.width()/height()/fps() for the rest
    ///  - dt:      seconds since the previous frame; 0 on the first,
    ///             capped at 0.25 so a stall doesn't make animations jump
    ///  - elapsed: seconds since the first frame (uncapped wall time)
    ///  - frame:   frame number, starting at 0
    /// Any callable works: a lambda, a functor, or `std::ref(obj)` to
    /// keep the state in your own object (no capture needed).
    template <typename F>
    void on_frame(F fn) {
        m_on_frame = [fn = std::move(fn)](Canvas& c,
                                          float dt,
                                          float elapsed,
                                          std::uint64_t frame) mutable {
            if constexpr(
                std::invocable<F&, Canvas&, float, float, std::uint64_t>)
                fn(c, dt, elapsed, frame);
            else if constexpr(std::invocable<F&, Canvas&, float, float>)
                fn(c, dt, elapsed);
            else if constexpr(std::invocable<F&, Canvas&, float>) fn(c, dt);
            else if constexpr(std::invocable<F&, Canvas&>) fn(c);
            else
                static_assert(std::invocable<F&, Canvas&>,
                              "on_frame handler must accept (Canvas&[, float "
                              "dt[, float elapsed[, uint64_t frame]]])");
        };
    }
    /// Removes the handler, returning to hand-driven mode.
    void clear_on_frame() { m_on_frame = nullptr; }

    /// Hook called by Renderer::push() before build(). Does nothing
    /// unless an on_frame() handler is set (so a hand-driven canvas keeps
    /// its contents). You normally never call this yourself.
    void prepare(layout::Region area) {
        if(!m_on_frame) return;
        fit(area);
        if(auto_clear) clear();
        const auto now = std::chrono::steady_clock::now();
        if(!m_start) m_start = now;
        const float dt =
            m_last ? std::chrono::duration<float>(now - *m_last).count() : 0.0f;
        m_last = now;
        m_on_frame(*this,
                   std::min(dt, 0.25f),
                   std::chrono::duration<float>(now - *m_start).count(),
                   m_frame_index++);
    }

    Canvas() = default;

    /// Constructs a canvas with a `width` x `height` grid of blank cells.
    Canvas(int width, int height) { resize(width, height); }

    /// @name Geometry
    /// @{
    [[nodiscard]] int width() const noexcept { return m_size.width; }
    [[nodiscard]] int height() const noexcept { return m_size.height; }
    [[nodiscard]] layout::Size size() const noexcept { return m_size; }

    /// Resizes the grid (negative values clamp to 0). Always resets
    /// every cell to blank, even if the size is unchanged.
    void resize(int w, int h) {
        m_size = {std::max(0, w), std::max(0, h)};
        m_cells.assign(static_cast<std::size_t>(m_size.width) *
                           static_cast<std::size_t>(m_size.height),
                       renderer::Cell{});
    }
    void resize(layout::Size s) { resize(s.width, s.height); }

    /// Resizes the grid to the *content* area of `area` (i.e. minus the
    /// border, if one is enabled in `flags`). Does nothing — and keeps
    /// the current contents — if the size already matches, so it is
    /// cheap to call every frame.
    void fit(layout::Region area) {
        auto s = area.size();
        if(active_border(flags)) {
            s.width -= 2;
            s.height -= 2;
        }
        s.width = std::max(0, s.width);
        s.height = std::max(0, s.height);
        if(s != m_size) resize(s);
    }
    /// @}

    /// @name Content
    /// @{

    /// Whether `p` (grid-local coordinates) is inside the grid.
    [[nodiscard]]
    bool contains(layout::Position p) const noexcept {
        return p.x >= 0 && p.y >= 0 && p.x < m_size.width &&
               p.y < m_size.height;
    }

    /// Cell at (x, y). Must be inside the grid (asserted in debug).
    [[nodiscard]]
    renderer::Cell& at(int x, int y) {
        assert(contains({x, y}));
        return m_cells[index(x, y)];
    }
    [[nodiscard]]
    const renderer::Cell& at(int x, int y) const {
        assert(contains({x, y}));
        return m_cells[index(x, y)];
    }

    /// Writes `cell` at (x, y); silently ignored if out of bounds, so
    /// a rasterizer doesn't need to clip itself.
    void set(int x, int y, renderer::Cell cell) {
        if(contains({x, y})) m_cells[index(x, y)] = cell;
    }
    void set(int x, int y, char32_t glyph,
             style::Color fg = {255, 255, 255, 255},
             style::Color bg = {0, 0, 0, 0}) {
        set(x,
            y,
            renderer::Cell{.glyph = glyph, .foreground = fg, .background = bg});
    }

    /// Sets every cell to `cell` (default: blank).
    void clear(renderer::Cell cell = {}) { fill(cell); }
    void fill(renderer::Cell cell) {
        std::fill(m_cells.begin(), m_cells.end(), cell);
    }

    /// The whole grid, row-major (`y * width() + x`), for fast rasterizing.
    [[nodiscard]]
    std::span<renderer::Cell> cells() noexcept {
        return m_cells;
    }
    [[nodiscard]]
    std::span<const renderer::Cell> cells() const noexcept {
        return m_cells;
    }
    /// @}

    /// @name Refresh rate
    /// @{

    /// Desired refresh rate hint, clamped to [0, kMaxFps]. 0 means
    /// app-driven (no periodic redraw wanted). This does nothing by
    /// itself; feed frame_interval_ms() to Input::set_tick_interval().
    void set_fps(int fps) noexcept { m_fps = std::clamp(fps, 0, kMaxFps); }
    [[nodiscard]] int fps() const noexcept { return m_fps; }

    /// Milliseconds between frames for the current fps, or 0 when
    /// fps() is 0 (app-driven).
    [[nodiscard]]
    int frame_interval_ms() const noexcept {
        return m_fps > 0 ? std::max(1, 1000 / m_fps) : 0;
    }

    static constexpr int kMaxFps = 240;
    /// @}

    /// Draws the border/title (if enabled), the optional background,
    /// then one DrawCell per non-empty cell, clipped to the content
    /// area. A cell is "empty" if its glyph is a space and its
    /// background is fully transparent, so it shows what's beneath.
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto content = area;
        if(auto border = active_border(flags)) {
            build_border(area, *border, cmd, border_color);
            build_title(area, title, border_color, cmd);
            content = inset(area);
        }

        auto [cw, ch] = content.size();
        if(cw <= 0 || ch <= 0) return;
        auto origin = content.position();

        if(background)
            cmd.push(renderer::Fill{
                content,
                renderer::Cell{.glyph = U' ', .background = *background}});

        const int w = std::min(cw, m_size.width);
        const int h = std::min(ch, m_size.height);
        for(int y = 0; y < h; y++) {
            for(int x = 0; x < w; x++) {
                const auto& c = m_cells[index(x, y)];
                if(c.glyph == renderer::kInvalidGlyph) continue;
                if(c.glyph == U' ' && c.background.a == 0) continue;
                cmd.push(renderer::DrawCell{{origin.x + x, origin.y + y}, c});
            }
        }
    }

private:
    [[nodiscard]]
    std::size_t index(int x, int y) const noexcept {
        return static_cast<std::size_t>(y) *
                   static_cast<std::size_t>(m_size.width) +
               static_cast<std::size_t>(x);
    }

    layout::Size m_size{};
    std::vector<renderer::Cell> m_cells;
    int m_fps{30};
    std::function<void(Canvas&, float, float, std::uint64_t)> m_on_frame;
    std::optional<std::chrono::steady_clock::time_point> m_start, m_last;
    std::uint64_t m_frame_index{0};
};

}  // namespace trux::component
