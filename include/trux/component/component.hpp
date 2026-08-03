#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/renderer/draw_command_buffer.hpp"

#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace trux::component {

[[nodiscard]]
inline std::vector<std::string_view>
to_views(const std::vector<std::string>& items) {
    std::vector<std::string_view> views;
    views.reserve(items.size());
    for(const auto& s : items) views.push_back(s);
    return views;
}

template <typename T>
concept ComponentType = requires(const T&                     component,
                                 layout::Region               area,
                                 renderer::DrawCommandBuffer& commands) {
    { component.flags } -> std::same_as<const ComponentFlags&>;
    { component.build(area, commands) } -> std::same_as<void>;
};

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
[[nodiscard]]
constexpr T operator|(T&& component, FlagModifier modifier)
    requires(!std::is_lvalue_reference_v<T>)
{
    modifier.apply(component);
    return std::move(component);
}

template <ComponentType T>
[[nodiscard]]
constexpr T& operator|(T& component, FlagModifier modifier) {
    modifier.apply(component);
    return std::move(component);
}

template <ComponentType T>
constexpr T& operator|=(T& component, FlagModifier modifier) {
    modifier.apply(component);
    return component;
}

[[nodiscard]]
constexpr FlagModifier operator|(FlagModifier lhs, FlagModifier rhs) {
    return FlagModifier{lhs.flag | rhs.flag};
}

inline constexpr FlagModifier BorderRounded{Flag::BorderRounded};
inline constexpr FlagModifier BorderSingle{Flag::BorderSingle};
inline constexpr FlagModifier BorderDouble{Flag::BorderDouble};
inline constexpr FlagModifier BorderBlock{Flag::BorderBlock};

inline constexpr FlagModifier Bold{Flag::Bold};
inline constexpr FlagModifier Italic{Flag::Italic};
inline constexpr FlagModifier Underline{Flag::Underline};

}  // namespace trux::component
