#pragma once

#include "trux/component/component_flags.hpp"

#include <string_view>
#include <vector>

namespace trux::component {
struct List {
    explicit constexpr List(std::vector<std::string_view> items)
        : items(items) {}

    std::vector<std::string_view> items = {};

    ComponentFlags flags{};
};
}  // namespace trux::component
