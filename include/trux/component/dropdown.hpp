#pragma once

#include "trux/component/component_flags.hpp"

#include <string_view>
#include <vector>

namespace trux::component {
struct Dropdown {
    explicit Dropdown(std::vector<std::string_view> options)
        : options(options) {}

    std::vector<std::string_view> options;

    size_t selected{};

    bool open{false};

    ComponentFlags flags{};
};
}  // namespace trux::component
