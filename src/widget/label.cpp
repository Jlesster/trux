#include <trux/widget/label.hpp>

using namespace trux;

void widget::Label::draw(renderer::Renderer& renderer, layout::Region region) {
    renderer.text(region, {0, 0}, m_text);
}
