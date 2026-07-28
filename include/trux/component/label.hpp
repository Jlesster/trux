#pragma once

#include "trux/component/component_flags.hpp"

#include <string_view>

namespace trux::component {
struct Label {
    explicit constexpr Label(std::string_view text) : text(text) {}

    std::string_view text = "";

    ComponentFlags flags{};
};
}  // namespace trux::component
