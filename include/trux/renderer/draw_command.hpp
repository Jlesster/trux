/// @file draw_command.hpp
/// @brief The recorded-command types components emit while building,
///        later replayed by Renderer::resolve() into the back buffer.

#pragma once

#include "trux/layout/region.hpp"
#include "trux/renderer/cell.hpp"

#include <string_view>
#include <variant>

namespace trux::renderer {

/// Draws a run of text starting at `position`, one glyph per column
/// (wide glyphs occupy multiple columns; see util::glyph_width()).
/// `text` must remain valid until the command buffer is resolved —
/// components typically point it at their own stored string data.
struct DrawText {
    /// Local position (relative to the region being built) of the
    /// first glyph.
    layout::Position position;
    /// UTF-8 text to draw. Not owned; must outlive command resolution.
    std::string_view text;

    /// Text attributes applied to the whole run.
    style::Style style{};
    /// Foreground color applied to the whole run.
    style::Color fg{255, 255, 255, 255};
    /// Background color applied to the whole run.
    style::Color bg{0, 0, 0, 0};
};

/// Draws a single fully-specified Cell at `position`.
struct DrawCell {
    layout::Position position;
    Cell             cell;
};

/// Fills every cell within `region` with `cell`, opaquely (bypasses
/// alpha blending — see Renderer's `put_opaque`).
struct Fill {
    layout::Region region;
    Cell           cell;
};

/// Pushes a clip rectangle: subsequent draw commands are restricted
/// to `region` until the matching ClearClip. Nested SetClip commands
/// intersect with the currently active clip rather than replacing it.
struct SetClip {
    layout::Region region;
    /// Whether the clipped content belongs to a modal layer (affects
    /// input routing/focus, not clipping itself — see
    /// Renderer::dispatch()).
    bool modal{false};
};

/// Pops the current clip rectangle, restoring whatever clip (if any)
/// was active before the matching SetClip.
struct ClearClip {};

/// A single recorded draw operation. Renderer::push()/`operator=`-driven
/// component builds append these to a DrawCommandBuffer, which
/// Renderer later replays in order to paint the back buffer.
using DrawCommand = std::variant<DrawText, DrawCell, Fill, SetClip, ClearClip>;
}  // namespace trux::renderer
