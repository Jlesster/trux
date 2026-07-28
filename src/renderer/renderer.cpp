#include "trux/layout/position.hpp"

#include <trux/renderer/renderer.hpp>

using namespace trux;

renderer::Renderer::Renderer(layout::Size size) : m_size(size) {
    m_front.resize(size);
    m_back.resize(size);
}

void renderer::Renderer::begin_draw() { m_back.clear(); }

void renderer::Renderer::end_draw() { m_front = m_back; }

void renderer::Renderer::resize(layout::Size size) {
    if(size == m_size) return;

    m_size = size;
    m_front.resize(size);
    m_back.resize(size);
}

renderer::CellBuffer& renderer::Renderer::back_buffer() noexcept {
    return m_back;
}

const renderer::CellBuffer& renderer::Renderer::back_buffer() const noexcept {
    return m_back;
}

void renderer::Renderer::put(layout::Position pos, Cell cell) {
    if(m_clip && !m_clip->contains(pos)) return;
    if(!m_back.contains(pos)) return;
    m_back.at(pos) = cell;
}

void renderer::Renderer::put(layout::Position pos, char32_t c) {
    put(pos, Cell{.glyph = c});
}

void renderer::Renderer::text(layout::Region   region,
                              layout::Position pos,
                              std::string_view text) {
    auto current = region.absolute(pos);
    for(char c : text) {
        put(current, static_cast<char32_t>(c));
        current.x++;
    }
}

void renderer::Renderer::set_clip(layout::Region region) { m_clip = region; }

void renderer::Renderer::clear_clip() { m_clip.reset(); }
