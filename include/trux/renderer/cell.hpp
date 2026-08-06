#pragma once

#include "trux/style/color.hpp"
#include "trux/style/style.hpp"

namespace trux::renderer {
inline constexpr char32_t kInvalidGlyph      = 0xFFFFFFFF;
inline constexpr char32_t kContinuationGlyph = 0xFFFFFFFE;

struct Cell {
    char32_t glyph{U' '};

    style::Color foreground{
        255,
        255,
        255,
        255,
    };
    style::Color background{
        0,
        0,
        0,
        0,
    };
    style::Style style{style::Style::None};

    constexpr auto operator<=>(const Cell&) const = default;
};

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
