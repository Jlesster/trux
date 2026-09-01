#include "test.hpp"

#include <cassert>
#include <trux/layout/layout.hpp>
#include <trux/renderer/renderer.hpp>

using namespace trux;

void render_test() {
    renderer::Renderer renderer({80, 25});

    renderer.begin_draw();
    renderer.put({10, 5}, U'X');

    auto& buffer = renderer.back_buffer();

    assert(buffer.at({10, 5}).glyph == U'X');
}

void text_test() {
    renderer::Renderer renderer({80, 25});
    auto               region = layout::init({20, 20});
    renderer.begin_draw();
    renderer.text(region, {5, 5}, "trux");

    auto& buffer = renderer.back_buffer();

    assert(buffer.at({5, 5}).glyph == U't');
    assert(buffer.at({6, 5}).glyph == U'r');
    assert(buffer.at({7, 5}).glyph == U'u');
    assert(buffer.at({8, 5}).glyph == U'x');
}

int main() {
    test::run("render test", render_test);
    test::run("text test", text_test);
    return test::summary();
}
