#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/input/event.hpp"
#include "trux/layout/region.hpp"
#include "trux/renderer/draw_command_buffer.hpp"
#include "trux/style/color.hpp"

#include <concepts>
#include <memory>
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
concept HasBorderColor = requires(T& t, style::Color c) { t.border_color = c; };

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
    return component;
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

struct ComponentBase {
    virtual ~ComponentBase()                             = default;
    virtual void build(layout::Region               area,
                       renderer::DrawCommandBuffer& cmd) = 0;
    virtual bool handle(const input::Event& event) { return false; }
    virtual const ComponentFlags& flags() const = 0;
};

template <ComponentType T> struct ComponentWrapper : ComponentBase {
    T inner;
    explicit ComponentWrapper(T c) : inner(std::move(c)) {}
    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) override {
        inner.build(area, cmd);
    }
    bool handle(const input::Event& e) override {
        if constexpr(input::Handleable<T>) return inner.handle(e);
        else return false;
    }
    const ComponentFlags& flags() const override { return inner.flags; }
};

struct Container {
    int            percent    = 50;
    bool           horizontal = true;
    ComponentFlags flags{};

    std::unique_ptr<ComponentBase> first;
    std::unique_ptr<ComponentBase> second;

    void build(layout::Region area, renderer::DrawCommandBuffer& cmd) const {
        auto split = horizontal ? area.h_split(percent) : area.v_split(percent);
        first->build(split[0], cmd);
        second->build(split[1], cmd);
    }
};

}  // namespace trux::component
