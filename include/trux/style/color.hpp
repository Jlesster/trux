#pragma once

#include <compare>
#include <cstdint>

namespace trux::style {
struct Color {
    uint8_t r{255};
    uint8_t g{255};
    uint8_t b{255};
    uint8_t a{255};

    constexpr auto operator<=>(const Color&) const = default;

    [[nodiscard]] static Color ActiveBorderColor() noexcept {
        return storage();
    }
    static void ActiveBorderColor(Color color) noexcept { storage() = color; }

private:
    [[nodiscard]]
    static Color& storage() noexcept {
        static Color color{255, 255, 255, 255};
        return color;
    }
};
}  // namespace trux::style
