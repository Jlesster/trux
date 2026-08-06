#include "test.hpp"

#include <cassert>
#include <trux/renderer/cell_buffer.hpp>

using namespace trux;

void test_write() {
    renderer::CellBuffer buffer;

    buffer.resize({80, 25});
    buffer.at({10, 5}).glyph = U'@';
    assert(buffer.at({10, 5}).glyph == U'@');
}

void test_clear() {
    renderer::CellBuffer buffer;

    buffer.resize({5, 5});
    buffer.at({2, 2}).glyph = U'X';
    buffer.clear();
    assert(buffer.at({2, 2}).glyph == U' ');
}

void test_resize_clears_old_content() {
    renderer::CellBuffer buffer;

    buffer.resize({5, 5});
    buffer.at({1, 1}).glyph = U'X';
    buffer.resize({5, 5});
    assert(buffer.at({1, 1}).glyph == U' ');
}

void test_blend_fully_transparent_src_preserves_dst() {
    auto result = renderer::blend({10, 20, 30, 255}, {0, 0, 0, 0});
    assert(result.r == 10 && result.g == 20 && result.b == 30 &&
           result.a == 255);
}

void test_blend_fully_opaque_src_overwrites_dst() {
    auto result = renderer::blend({10, 20, 30, 255}, {200, 100, 50, 255});
    assert(result.r == 200 && result.g == 100 && result.b == 50 &&
           result.a == 255);
}

void test_blend_transparent_over_transparent_stays_transparent() {
    auto result = renderer::blend({0, 0, 0, 0}, {0, 0, 0, 0});
    assert(result.a == 0);
}

int main() {
    test::run("write", test_write);
    test::run("clear", test_clear);
    test::run("resize_clear", test_resize_clears_old_content);
    test::run("fully_transparent_blend_test",
              test_blend_fully_transparent_src_preserves_dst);
    test::run("fully_opaque_src_test",
              test_blend_fully_opaque_src_overwrites_dst);
    test::run("transparent_over_transparent_test",
              test_blend_transparent_over_transparent_stays_transparent);
}
