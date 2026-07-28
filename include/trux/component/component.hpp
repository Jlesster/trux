#pragma once

#include "trux/component/component_flags.hpp"

#include <concepts>
namespace trux::component {

struct Component {
    ComponentFlags flags{};
};

template <typename T>
concept ComponentType = requires(T component) { component.flags; };

struct FlagModifier {
    Flag flag;

    template <ComponentType T> constexpr void apply(T& target) const {
        target.flags.add(flag);
    }

    template <ComponentType T> constexpr void clear(T& target) const {
        target.flags.remove(flag);
    }
};

template <ComponentType T>
constexpr T operator|(T component, FlagModifier modifier) {
    modifier.apply(component);
    return component;
}

inline constexpr FlagModifier BorderRounded{Flag::BorderRounded};
inline constexpr FlagModifier BorderSingle{Flag::BorderSingle};
inline constexpr FlagModifier BorderDouble{Flag::BorderDouble};
inline constexpr FlagModifier BorderBlock{Flag::BorderBlock};

inline constexpr FlagModifier Bold{Flag::Bold};
inline constexpr FlagModifier Italic{Flag::Italic};
inline constexpr FlagModifier Underline{Flag::Underline};

}  // namespace trux::component
