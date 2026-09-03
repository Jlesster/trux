/// @file color.hpp
/// @brief RGBA color type and the global active-border-color setting.

#pragma once

#include <compare>
#include <cstdint>

namespace trux::style {

/// An 8-bit-per-channel RGBA color, defaulting to opaque white.
///
/// Alpha (`a`) is carried for API completeness but terminal output
/// is not itself alpha-blended; renderer/terminal backends are
/// expected to treat colors as opaque unless documented otherwise.
struct Color {
    uint8_t r{255};
    uint8_t g{255};
    uint8_t b{255};
    uint8_t a{255};

    /// Compares colors by (r, g, b, a).
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
