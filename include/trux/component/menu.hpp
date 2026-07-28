#pragma once

#include "trux/component/component_flags.hpp"
#include "trux/style/style.hpp"

#include <string_view>
#include <vector>

namespace trux::component {
struct Menu {
    explicit constexpr Menu(std::vector<std::string_view> items)
        : items(items) {}

    std::vector<std::string_view> items{};

    style::Style   selected_style{};
    ComponentFlags flags{};

    int selected = 0;
};
}  // namespace trux::component
