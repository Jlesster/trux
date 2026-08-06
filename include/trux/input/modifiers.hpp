#pragma once

#include <compare>
#include <cstdint>

namespace trux::input {

enum class Mod : uint8_t {
    None  = 0,
    Shift = 1 << 0,
    Alt   = 1 << 1,
    Ctrl  = 1 << 2,
    Super = 1 << 3,
};

struct Modifiers {
    uint8_t value{};

    constexpr void add(Mod m) noexcept { value |= static_cast<uint8_t>(m); }
    [[nodiscard]]
    constexpr bool has(Mod m) const noexcept {
        return value & static_cast<uint8_t>(m);
    }
    constexpr auto operator<=>(const Modifiers&) const = default;
};

[[nodiscard]]
constexpr Modifiers decode_modifier_param(int param) noexcept {
    Modifiers m{};
    if(param <= 1) return m;
    int bits = param - 1;
    if(bits & 1) m.add(Mod::Shift);
    if(bits & 2) m.add(Mod::Alt);
    if(bits & 4) m.add(Mod::Ctrl);
    if(bits & 8) m.add(Mod::Super);
    return m;
}

[[nodiscard]]
constexpr Modifiers with(Mod m) noexcept {
    Modifiers mods{};
    mods.add(m);
    return mods;
}

}  // namespace trux::input
