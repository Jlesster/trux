/// @file border.hpp
/// @brief Border drawing and classification: glyph tables for the
///        single/rounded/double/block border styles, building a
///        border into draw commands, and classifying an existing
///        glyph back into its style/direction mask (used by
///        Renderer::coalesce_borders() to join adjacent components'
///        borders).

#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/layout/region.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/util/util.hpp"

#include <optional>

namespace trux::component {

/// A single edge direction, as a bit, used to build up junction masks
/// (e.g. `South | East` for a top-left corner).
enum BorderDir : uint8_t { North = 1, South = 2, East = 4, West = 8 };

/// The full set of line and junction glyphs for one border style
/// (single, rounded, or double), indexed by direction mask via
/// glyph_for_mask().
struct JunctionSet {
    char32_t horizontal, vertical;
    char32_t top_left, top_right, bottom_left, bottom_right;
    char32_t t_down, t_up, t_right, t_left, cross;
};

/// The JunctionSet of glyphs for border style `style` (BorderSingle,
/// BorderRounded, or BorderDouble; anything else falls back to
/// single). Used when coalescing adjacent border segments into
/// junction characters.
[[nodiscard]]
inline const JunctionSet& junctions_for(Flag style) {
    static const JunctionSet single{
        U'─', U'│', U'┌', U'┐', U'└', U'┘', U'┬', U'┴', U'├', U'┤', U'┼'};
    static const JunctionSet& rounded{
        U'─', U'│', U'╭', U'╮', U'╰', U'╯', U'┬', U'┴', U'├', U'┤', U'┼'};
    static const JunctionSet dbl{
        U'═', U'║', U'╔', U'╗', U'╚', U'╝', U'╦', U'╩', U'╠', U'╣', U'╬'};
    if(style == Flag::BorderDouble) return dbl;
    if(style == Flag::BorderRounded) return rounded;
    return single;
};

/// The four corner and two line glyphs needed to draw one rectangular
/// border of a single style, as returned by glyphs_for().
struct BorderGlyphs {
    char32_t top_left, top_right, bottom_left, bottom_right;
    char32_t horizontal, vertical;
};

/// Looks up the glyph in `j` for a junction with the given
/// combination of BorderDir bits set (e.g. `North | South` for a
/// vertical line, `South | East | West` for a top T-junction).
/// Returns U'\0' for an unrecognized mask (0, 1, 2, 4, or 8 alone).
[[nodiscard]]
inline char32_t glyph_for_mask(const JunctionSet& j, uint8_t mask) {
    switch(mask) {
        case North | South: return j.vertical;
        case East | West: return j.horizontal;
        case South | East: return j.top_left;
        case South | West: return j.top_right;
        case North | East: return j.bottom_left;
        case North | West: return j.bottom_right;
        case South | East | West: return j.t_down;
        case North | East | West: return j.t_up;
        case North | South | East: return j.t_right;
        case North | South | West: return j.t_left;
        case North | South | East | West: return j.cross;
        default: return U'\0';
    }
}

/// Result of classify_border_glyph(): whether a glyph is a
/// recognized border character, and if so which style and direction
/// mask it represents.
struct Classified {
    bool is_border{false};
    Flag style{Flag::BorderSingle};
    uint8_t mask{0};
};

/// Identifies whether `glyph` is one of the border-drawing characters
/// this library uses, and if so, its style and BorderDir mask.
/// Used by Renderer::coalesce_borders() to recognize adjacent border
/// segments belonging to neighboring components so they can be joined
/// into a single junction glyph.
[[nodiscard]]
inline Classified classify_border_glyph(char32_t glyph) {
    /// One entry in the glyph-to-classification lookup table.
    struct Entry {
        char32_t ch;
        Flag style;
        uint8_t mask;
    };
    static constexpr Entry table[] = {
        {U'─', Flag::BorderSingle, East | West},
        {U'│', Flag::BorderSingle, North | South},
        {U'┌', Flag::BorderSingle, South | East},
        {U'┐', Flag::BorderSingle, South | West},
        {U'└', Flag::BorderSingle, North | East},
        {U'┘', Flag::BorderSingle, North | West},
        {U'┬', Flag::BorderSingle, South | East | West},
        {U'┴', Flag::BorderSingle, North | East | West},
        {U'├', Flag::BorderSingle, North | South | East},
        {U'┤', Flag::BorderSingle, North | South | West},
        {U'┼', Flag::BorderSingle, North | South | East | West},
        {U'╭', Flag::BorderRounded, South | East},
        {U'╮', Flag::BorderRounded, South | West},
        {U'╰', Flag::BorderRounded, North | East},
        {U'╯', Flag::BorderRounded, North | West},
        {U'═', Flag::BorderDouble, East | West},
        {U'║', Flag::BorderDouble, North | South},
        {U'╔', Flag::BorderDouble, South | East},
        {U'╗', Flag::BorderDouble, South | West},
        {U'╚', Flag::BorderDouble, North | East},
        {U'╝', Flag::BorderDouble, North | West},
        {U'╦', Flag::BorderDouble, South | East | West},
        {U'╩', Flag::BorderDouble, North | East | West},
        {U'╠', Flag::BorderDouble, North | South | East},
        {U'╣', Flag::BorderDouble, North | South | West},
        {U'╬', Flag::BorderDouble, North | South | East | West},
    };
    for(auto& e : table)
        if(e.ch == glyph) return {true, e.style, e.mask};
    return {};
}

/// The corner/line glyphs to draw for border style `border`
/// (BorderRounded, BorderDouble, BorderBlock, or the BorderSingle
/// default for anything else).
[[nodiscard]]
inline BorderGlyphs glyphs_for(Flag border) {
    switch(border) {
        case Flag::BorderRounded: return {U'╭', U'╮', U'╰', U'╯', U'─', U'│'};
        case Flag::BorderDouble: return {U'╔', U'╗', U'╚', U'╝', U'═', U'║'};
        case Flag::BorderBlock: return {U'█', U'█', U'█', U'█', U'█', U'█'};
        default:  // BorderSingle
            return {U'┌', U'┐', U'└', U'┘', U'─', U'│'};
    }
}

/// The border style requested by `flags`, or std::nullopt if none of
/// the Border* flags is set. When multiple are set (which shouldn't
/// normally happen), checks Rounded, then Double, then Block, then Single.
[[nodiscard]]
inline std::optional<Flag> active_border(const ComponentFlags& flags) {
    if(flags.has(Flag::BorderRounded)) return Flag::BorderRounded;
    if(flags.has(Flag::BorderDouble)) return Flag::BorderDouble;
    if(flags.has(Flag::BorderBlock)) return Flag::BorderBlock;
    if(flags.has(Flag::BorderSingle)) return Flag::BorderSingle;
    return std::nullopt;
}
/// Optional title for components to use, it has independent styling
/// across fg, bg, active_fg, active_bg and attribs.
struct Title {
    enum class Align : uint8_t { Left, Center, Right };
    enum class Edge : uint8_t { Top, Bottom };

    std::string text;
    Align align{Align::Left};
    Edge edge{Edge::Top};

    /// Resting style. unset falls back to border color
    /// which Renderer::push() already switches on focus.
    std::optional<style::Color> fg, bg;
    style::Style attribs{style::Style::None};

    /// Overrides used only while focused.
    /// Each falls back to the resting value
    std::optional<style::Color> active_fg, active_bg;
    std::optional<style::Style> active_attribs;

    /// Written by Renderer::push() every frame, not to be manually set
    bool active{false};

    /// Keeps `title = "x"` working. A user-declared assignment operator
    /// doesn't stop this from being an aggregate so designated init still works
    Title& operator=(std::string s) {
        text = std::move(s);
        return *this;
    }
};

/// Records DrawCell commands outlining `area` with `border`-style
/// glyphs in `color`. A no-op if `area` is smaller than 2x2 (no room
/// for both a border and any content).
inline void build_border(layout::Region area, Flag border,
                         renderer::DrawCommandBuffer& cmd,
                         style::Color color = {255, 255, 255, 255}) {
    auto [w, h] = area.size();
    if(w < 2 || h < 2) return;

    auto pos = area.position();
    auto g = glyphs_for(border);
    int right = pos.x + w - 1;
    int bottom = pos.y + h - 1;

    auto cell = [&](char32_t c) {
        return renderer::Cell{.glyph = c, .foreground = color};
    };

    cmd.push(renderer::DrawCell{{pos.x, pos.y}, cell(g.top_left)});
    cmd.push(renderer::DrawCell{{right, pos.y}, cell(g.top_right)});
    cmd.push(renderer::DrawCell{{pos.x, bottom}, cell(g.bottom_left)});
    cmd.push(renderer::DrawCell{{right, bottom}, cell(g.bottom_right)});

    for(int x = pos.x + 1; x < right; x++) {
        cmd.push(renderer::DrawCell{{x, pos.y}, cell(g.horizontal)});
        cmd.push(renderer::DrawCell{{x, bottom}, cell(g.horizontal)});
    }
    for(int y = pos.y + 1; y < bottom; y++) {
        cmd.push(renderer::DrawCell{{pos.x, y}, cell(g.vertical)});
        cmd.push(renderer::DrawCell{{right, y}, cell(g.vertical)});
    }
}

/// Records Title draw commands, building the declared `Text` with `Style`
/// glyphs in `color`. A no-op if `Text` is empty.
inline void build_title(layout::Region area, const Title& t,
                        style::Color border_color,
                        renderer::DrawCommandBuffer& cmd) {
    if(t.text.empty()) return;
    auto [w, h] = area.size();
    if(w < 5 || h < 2) return;

    const int avail =
        w - 4;  // minus 2 corners, minus 1 padding space each side
    const std::string_view text = t.text;
    const int total = static_cast<int>(text.size());

    auto glyph_cols = [&](int i) {
        auto d = util::decode_utf8(text.substr(static_cast<size_t>(i)));
        return std::max(1, util::glyph_width(d ? d->first : U'?'));
    };

    int full = 0;
    for(int i = 0; i < total; i = util::next_boundary(text, i))
        full += glyph_cols(i);

    const bool ellipsis = full > avail;
    const int limit = ellipsis ? avail - 1 : avail;  // reserve a cell for "…"

    int bytes = 0, cols = 0;
    while(bytes < total) {
        int gw = glyph_cols(bytes);
        if(cols + gw > limit) break;
        cols += gw;
        bytes = util::next_boundary(text, bytes);
    }
    const int span = cols + (ellipsis ? 1 : 0);  // columns of visible text
    auto pos = area.position();
    int right = pos.x + w - 1;
    int y = (t.edge == Title::Edge::Top) ? pos.y : pos.y + h - 1;

    int x0;  // left edge of the block, including its leading padding space
    switch(t.align) {
        case Title::Align::Left: x0 = pos.x + 1; break;
        case Title::Align::Right: x0 = right - span - 2; break;
        case Title::Align::Center:
            x0 = pos.x + 1 + (w - 2 - (span + 2)) / 2;
            break;
    }
    auto fg = (t.active && t.active_fg) ? *t.active_fg
              : t.fg                    ? *t.fg
                                        : border_color;
    auto bg = (t.active && t.active_bg) ? *t.active_bg
              : t.bg                    ? *t.bg
                                        : style::Color{0, 0, 0, 0};
    auto st = (t.active && t.active_attribs) ? *t.active_attribs : t.attribs;

    auto put = [&](int x, std::string_view s) {
        cmd.push(renderer::DrawText{
            .position = {x, y}, .text = s, .style = st, .fg = fg, .bg = bg});
    };

    // Title block must be one opaque Fill: Renderer::put() never overwrites
    // a glyph with a space, so a DrawText " " (padding or an interior space
    // in the title) leaves the border showing. Fill first, then draw the
    // visible glyphs on top.
    cmd.push(renderer::Fill{
        layout::Region{{x0, y}, {span + 2, 1}},
        renderer::Cell{
            .glyph = U' ', .foreground = fg, .background = bg, .style = st}});

    put(x0 + 1, text.substr(0, static_cast<size_t>(bytes)));
    if(ellipsis) put(x0 + 1 + cols, "…");
}

/// The content area remaining inside `area` after a 1-cell border is
/// drawn around it (i.e. `area` shrunk by one cell on each side).
[[nodiscard]]
inline layout::Region inset(layout::Region area) {
    auto pos = area.position();
    auto size = area.size();
    return layout::Region{{pos.x + 1, pos.y + 1},
                          {size.width - 2, size.height - 2}};
}

}  // namespace trux::component
