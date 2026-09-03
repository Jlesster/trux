/// @file cell.hpp
/// @brief The Cell type (one terminal character cell's contents) and
///        alpha-blending helper.

#pragma once

#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

namespace trux::renderer {

/// Sentinel glyph marking a cell as needing a full redraw, used to
/// force every cell to be treated as changed (see
/// CellBuffer::invalidate()). Never a real drawn glyph.
inline constexpr char32_t kInvalidGlyph = 0xFFFFFFFF;

/// Sentinel glyph marking a cell as the trailing continuation of a
/// wide (multi-column) glyph drawn in the preceding cell. Skipped
/// during diffing/output since it carries no independent content —
/// see CellBuffer::diff().
inline constexpr char32_t kContinuationGlyph = 0xFFFFFFFE;

/// The full contents of one terminal cell: a glyph plus its
/// foreground/background color and text attributes.
///
/// Defaults to a space glyph on a fully transparent black background
/// with white (opaque) foreground and no style — i.e. an "empty" cell
/// that blends invisibly over whatever is beneath it (see blend()).
struct Cell {
    /// Unicode code point to display. May be `kInvalidGlyph` or
    /// `kContinuationGlyph` as sentinels rather than a real glyph.
    char32_t glyph{U' '};

    /// Foreground (text) color.
    style::Color foreground{
        255,
        255,
        255,
        255,
    };
    /// Background color. Alpha 0 by default so an empty cell doesn't
    /// obscure content drawn beneath it when blended.
    style::Color background{
        0,
        0,
        0,
        0,
    };
    /// Text attribute bitmask (bold, underline, etc.).
    style::Style style{style::Style::None};

    /// Compares all fields for equality; used by CellBuffer::diff()
    /// to detect changed cells between frames.
    constexpr auto operator<=>(const Cell&) const = default;
};

/// Alpha-composites `src` over `dst` using standard "over" blending,
/// per color channel, using each color's alpha as its opacity.
///
/// Returns fully transparent black if the combined alpha is zero.
/// Used by Renderer::put() so that translucent draws layer correctly
/// over previously drawn content instead of fully overwriting it.
///
/// @param dst Color currently in the buffer (the "background" layer).
/// @param src Color being drawn on top.
inline style::Color blend(style::Color dst, style::Color src) {
    float sa    = src.a / 255.0f;
    float da    = dst.a / 255.0f;
    float out_a = sa + da * (1.0f - sa);

    if(out_a <= 0.0f) return {0, 0, 0, 0};

    float r = (src.r * sa + dst.r * da * (1.0f - sa)) / out_a;
    float g = (src.g * sa + dst.g * da * (1.0f - sa)) / out_a;
    float b = (src.b * sa + dst.b * da * (1.0f - sa)) / out_a;

    return {static_cast<uint8_t>(r),
            static_cast<uint8_t>(g),
            static_cast<uint8_t>(b),
            static_cast<uint8_t>(out_a * 255.0f)};
}

}  // namespace trux::renderer
