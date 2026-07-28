#pragma once

#include "trux/renderer/renderer.hpp"

#include <string_view>
#include <trux/widget/widget.hpp>

namespace trux::widget {

class Label : public Widget {
public:
    explicit Label(std::string_view text) : m_text(text) {}

    void draw(renderer::Renderer&, layout::Region) override;

private:
    std::string_view m_text;
};
}  // namespace trux::widget
