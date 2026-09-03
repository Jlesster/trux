/// @file util.hpp
/// @brief UTF-8 encode/decode, terminal glyph width, word/grapheme
///        boundary helpers, and SGR escape code generation.
///
/// Most of these are the low-level primitives that Terminal,
/// Renderer, and text-editing components (e.g. TextInput) build on;
/// application code rarely needs to call them directly.

#pragma once

#include "trux/renderer/cell.hpp"
#include "trux/style/style.hpp"

#include <format>
#include <optional>
#include <string_view>
#include <utility>

namespace trux::util {

/// Whether `cp` should be treated as part of a "word" for
/// word-boundary navigation (see prev_word_boundary()/
/// next_word_boundary()): ASCII alphanumerics, underscore, or any
/// non-ASCII code point.
[[nodiscard]]
inline bool is_word_char(char32_t cp) noexcept {
    return (cp >= '0' && cp <= '9') || (cp >= 'a' && cp <= 'z') ||
           (cp >= 'A' && cp <= 'Z') || cp == '_' || cp > 0x7F;
}

/// Builds the SGR (Select Graphic Rendition) escape code parameter
/// string for `cell` — foreground color (always as 24-bit truecolor),
/// background color (only if `cell.background.a > 0`), and any set
/// style attributes — ready to be embedded in `"\x1b[{}m"`.
///
/// @return The parameter portion only (e.g. `"0;38;2;255;255;255;1"`),
///         without the leading `\x1b[` or trailing `m`.
[[nodiscard]]
inline std::string sgr_codes(const trux::renderer::Cell& cell) {

    auto has = [style = static_cast<uint8_t>(cell.style)](style::Style s) {
        return style & static_cast<uint8_t>(s);
    };

    std::string codes = "0;";

    codes += std::format("38;2;{};{};{}",
                         cell.foreground.r,
                         cell.foreground.g,
                         cell.foreground.b);

    if(cell.background.a > 0) {
        codes += std::format(";48;2;{};{};{}",
                             cell.background.r,
                             cell.background.g,
                             cell.background.b);
    }

    if(has(style::Style::Bold)) codes += ";1";
    if(has(style::Style::Dim)) codes += ";2";
    if(has(style::Style::Italic)) codes += ";3";
    if(has(style::Style::Underline)) codes += ";4";
    if(has(style::Style::Blink)) codes += ";5";
    if(has(style::Style::Reverse)) codes += ";7";
    if(has(style::Style::Strike)) codes += ";9";

    return codes;
}

[[nodiscard]]
inline std::optional<std::pair<char32_t, int>> decode_utf8(std::string_view s) {
    if(s.empty()) return std::nullopt;
    unsigned char b0 = s[0];

    if(b0 < 0x80) return std::pair{char32_t{b0}, 1};
    int len = (b0 & 0xE0) == 0xC0   ? 2
              : (b0 & 0xF0) == 0xE0 ? 3
              : (b0 & 0xF8) == 0xF0 ? 4
                                    : 0;

    if(len == 0 || s.size() < static_cast<size_t>(len)) return std::nullopt;

    char32_t cp = b0 & (0xFF >> (len + 1));
    for(int i = 1; i < len; i++)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3F);
    return std::pair{cp, len};
}

[[nodiscard]]
int glyph_width(char32_t cp) noexcept;

[[nodiscard]]
inline std::string encode_utf8(char32_t cp) {
    std::string out;

    if(cp <= 0x7F) {
        out += static_cast<char>(cp);
    } else if(cp <= 0x7FF) {
        out += static_cast<char>(0xC0 | (cp >> 6));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if(cp <= 0xFFFF) {
        out += static_cast<char>(0xE0 | (cp >> 12));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    } else if(cp <= 0x10FFFF) {
        out += static_cast<char>(0xF0 | (cp >> 18));
        out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
        out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
        out += static_cast<char>(0x80 | (cp & 0x3F));
    }
    return out;
}

[[nodiscard]]
inline int prev_boundary(std::string_view s, int cursor) noexcept {
    if(cursor <= 0) return 0;
    int i = cursor - 1;
    while(i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) i--;
    return i;
}

[[nodiscard]]
inline int next_boundary(std::string_view s, int cursor) noexcept {
    if(cursor >= static_cast<int>(s.size())) return static_cast<int>(s.size());
    if(auto decoded = decode_utf8(s.substr(static_cast<size_t>(cursor))))
        return cursor + decoded->second;
    return cursor + 1;
}

[[nodiscard]]
inline int prev_word_boundary(std::string_view s, int cursor) noexcept {
    int i = cursor;
    while(i > 0) {
        int  p = prev_boundary(s, i);
        auto d = decode_utf8(s.substr(static_cast<size_t>(p)));
        if(!d || is_word_char(d->first)) break;
        i = p;
    }
    while(i > 0) {
        int  p = prev_boundary(s, i);
        auto d = decode_utf8(s.substr(static_cast<size_t>(p)));
        if(!d || !is_word_char(d->first)) break;
        i = p;
    }
    return i;
}

[[nodiscard]]
inline int next_word_boundary(std::string_view s, int cursor) noexcept {
    int n = static_cast<int>(s.size());
    int i = cursor;
    while(i < n) {
        auto d = decode_utf8(s.substr(static_cast<size_t>(i)));
        if(!d || is_word_char(d->first)) break;
        i += d->second;
    }

    while(i < n) {
        auto d = decode_utf8(s.substr(static_cast<size_t>(i)));
        if(!d || !is_word_char(d->first)) break;
        i += d->second;
    }
    return i;
}

}  // namespace trux::util
