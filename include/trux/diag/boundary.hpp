/// @file boundary.hpp
/// @brief The "error boundary" widget: what a component is replaced
///        with when its build() throws. Shows which component failed
///        and why, inside its own area, instead of killing the app.

#pragma once

#include "trux/component/border.hpp"
#include "trux/diag/diag.hpp"
#include "trux/layout/region.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"
#include "trux/util/util.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace trux::diag {

namespace detail {
/// Splits `text` into lines of at most `width` columns, breaking at
/// spaces where possible. Counts code points (good enough for errors).
inline std::vector<std::string> wrap(std::string_view text, int width) {
    std::vector<std::string> lines;
    if(width <= 0) return lines;
    std::string cur;
    int cols = 0;
    auto flush = [&] {
        lines.push_back(cur);
        cur.clear();
        cols = 0;
    };
    for(size_t i = 0; i < text.size();) {
        auto d = util::decode_utf8(text.substr(i));
        int len = d ? d->second : 1;
        char32_t cp = d ? d->first : U'?';
        if(cp == U'\n') {
            flush();
            i += len;
            continue;
        }
        int w = std::max(1, util::glyph_width(cp));
        if(cols + w > width) {
            auto sp = cur.rfind(' ');
            if(sp != std::string::npos && sp > 0) {
                std::string rest = cur.substr(sp + 1);
                cur.resize(sp);
                flush();
                cur = rest;
                cols = static_cast<int>(rest.size());
            } else flush();
        }
        cur.append(text.substr(i, len));
        cols += w;
        i += len;
    }
    if(!cur.empty()) flush();
    return lines;
}
}  // namespace detail

/// Fills `area` with an error box: red border, "error · <source>"
/// title, the message wrapped inside, and a pointer to the log. Falls
/// back to a one-line strip when `area` is too small for a border.
/// All text is stored via cmd.own(), so it is valid for the frame.
inline void build_error_box(layout::Region area, std::string_view source,
                            std::string_view message,
                            renderer::DrawCommandBuffer& cmd) {
    auto [w, h] = area.size();
    if(w <= 0 || h <= 0) return;
    auto pos = area.position();

    const style::Color red = style::Color::Ansi(1);
    const style::Color dim = style::Color::Ansi(8);

    auto text = [&](int x, int y, std::string s, style::Color fg,
                    style::Style st = style::Style::None) {
        cmd.push(renderer::DrawText{
            .position = {x, y}, .text = cmd.own(std::move(s)), .style = st, .fg = fg});
    };

    cmd.push(renderer::Fill{area, renderer::Cell{}});

    if(w < 6 || h < 3) {
        std::string line = std::format("! {}: {}", source, message);
        if(static_cast<int>(line.size()) > w) line.resize(static_cast<size_t>(w));
        text(pos.x, pos.y, std::move(line), red, style::Style::Bold);
        return;
    }

    component::build_border(area, component::Flag::BorderSingle, cmd, red);
    // ASCII on purpose, so size() == columns. The Fill is needed because
    // Renderer::put() never overwrites a glyph with a space (see build_title).
    std::string title = std::format(" error: {} ", source);
    if(static_cast<int>(title.size()) > w - 4) title.resize(static_cast<size_t>(w - 4));
    cmd.push(renderer::Fill{
        layout::Region{{pos.x + 2, pos.y}, {static_cast<int>(title.size()), 1}},
        renderer::Cell{.glyph = U' ', .foreground = red}});
    text(pos.x + 2, pos.y, std::move(title), red, style::Style::Bold);

    const int inner_w = w - 4;  // border + 1 padding each side
    int rows = h - 2;
    auto lines = detail::wrap(message, inner_w);
    const bool footer = rows >= 3;  // keep a row for the log pointer
    int body_rows = footer ? rows - 1 : rows;

    if(static_cast<int>(lines.size()) > body_rows && body_rows > 0) {
        lines.resize(static_cast<size_t>(body_rows));
        lines.back().resize(
            std::min(lines.back().size(), static_cast<size_t>(std::max(0, inner_w - 1))));
        lines.back() += "…";
    }
    for(size_t i = 0; i < lines.size(); i++)
        text(pos.x + 2, pos.y + 1 + static_cast<int>(i), lines[i], style::Color::Default());
    if(footer)
        text(pos.x + 2, pos.y + h - 2,
             std::format("log: {}", Log::get().path()).substr(0, static_cast<size_t>(inner_w)),
             dim);
}

}  // namespace trux::diag
