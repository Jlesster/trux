#pragma once

#include "trux/input/event.hpp"

#include <trux/layout/region.hpp>
#include <trux/renderer/renderer.hpp>

namespace trux::widget {

class Widget {
public:
    virtual ~Widget() = default;

    virtual void draw(renderer::Renderer&, layout::Region) = 0;

    virtual bool handle(input::Event&) { return false; }
};
}  // namespace trux::widget
