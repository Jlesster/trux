#include <trux/renderer/renderer.hpp>

using namespace trux;

void renderer::Renderer::begin_draw() { m_back.clear(); }

void renderer::Renderer::end_draw() { m_front = m_back; }
