#pragma once

#include "trux/layout/region.hpp"
#include "trux/renderer/cell.hpp"

#include <string_view>
#include <variant>

namespace trux::renderer {

struct DrawText {
    layout::Position position;
    std::string_view text;

    style::Style style{};
    style::Color fg{255, 255, 255, 255};
    style::Color bg{0, 0, 0, 0};
};

struct DrawCell {
    layout::Position position;
    Cell             cell;
};

struct Fill {
    layout::Region region;
    Cell           cell;
};

struct SetClip {
    layout::Region region;
    bool           modal{false};
};

struct ClearClip {};

using DrawCommand = std::variant<DrawText, DrawCell, Fill, SetClip, ClearClip>;
}  // namespace trux::renderer
