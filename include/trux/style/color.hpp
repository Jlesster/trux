/// @file color.hpp
/// @brief Colro type (truecolor RGBA, terminal palette index, ir the terminal
/// default) and the global active-border-color setting.

#pragma once

#include <compare>
#include <cstdint>

namespace trux::style {

/// How a Color is resolved when it reaches the terminal

enum class ColorKind : uint8_t {
    /// 24-bit truecolor from the 'r'/'g'/'b' channels.
    Rgb,
    /// An index into the terminals own palette: 0-15 are the ANSI colors
    /// (so they fillow the user's therminal theme), 16-231 the 6x6x6 color
    /// cube,
    /// 232-255 the grayscale ramp.
    Indexed,
    /// Wahtever the terminal's default foreground/background iis.
    Default,
};

/// A color for a cell's forground or background.
///
/// Three flavors, picked by constructor:
///
/// @code{.cpp}
/// Color{160, 0, 160} // Truecolor, fixed regardless of term theme
/// Color::Ansi(1)     // palette index 1 (red) -- follows terminal theme
/// @endcode
///
/// 'r'/'g'/'b'/'a' are only meaningful for 'ColorKind::Rgb'; 'index' is only
/// meaningful for 'ColorKind::Indexed', Alpha is carried for API completeness
/// but terminal output is not alpha blended;
/// see render::blend() for how non-RGB colors layer
struct Color {
    uint8_t r{255};
    uint8_t g{255};
    uint8_t b{255};
    uint8_t a{255};
    ColorKind kind{ColorKind::Rgb};
    uint8_t index{0};

    /// Opaque white truecolor.
    constexpr Color() = default;

    /// Truecolor from 8-bit channels. Alpha defaults to opaque.
    constexpr Color(uint8_t red, uint8_t green, uint8_t blue,
                    uint8_t alpha = 255) noexcept
        : r(red),
          g(green),
          b(blue),
          a(alpha) {}

    /// Terminal palette color (0-255). Explicit so 'Color c = 5';
    /// doesn't silently mean "palette entry 5". Prefer Ansi() at call
    // sites where the intent should be obvious.
    constexpr explicit Color(uint8_t palette_index) noexcept
        : r(0),
          g(0),
          b(0),
          kind(ColorKind::Indexed),
          index(palette_index) {}

    /// Named form of the palette-index constructor.
    [[nodiscard]] static constexpr Color Ansi(uint8_t palette_index) noexcept {
        return Color(palette_index);
    }

    /// The terminal's default foreground/background color.
    [[nodiscard]] static constexpr Color Default() noexcept {
        Color c{0, 0, 0, 255};
        c.kind = ColorKind::Default;
        return c;
    }

    [[nodiscard]] constexpr bool is_rgb() const noexcept {
        return kind == ColorKind::Rgb;
    }
    [[nodiscard]] constexpr bool is_indexed() const noexcept {
        return kind == ColorKind::Indexed;
    }
    [[nodiscard]] constexpr bool is_default() const noexcept {
        return kind == ColorKind::Default;
    }

    /// Compares colors field by field (kind included)
    constexpr auto operator<=>(const Color&) const = default;

    /// Gets the process-wide color used to draw a focused/active
    /// border (see e.g. component::Border). Defaults to opaque white.
    [[nodiscard]] static Color ActiveBorderColor() noexcept {
        return storage();
    }

    /// Sets the process-wide active border color. Affects all
    /// subsequently drawn active borders.
    ///
    /// @param color New color to use for active borders.
    static void ActiveBorderColor(Color color) noexcept { storage() = color; }

private:
    /// Backing storage for the ActiveBorderColor global (a function-local
    /// static, so it is initialized lazily and safely across translation
    /// units).
    [[nodiscard]]
    static Color& storage() noexcept {
        static Color color{255, 255, 255, 255};
        return color;
    }
};
}  // namespace trux::style
