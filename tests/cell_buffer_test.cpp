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

int main() {
    test::run("write", test_write);
    test::run("clear", test_clear);
    test::run("resize_clear", test_resize_clears_old_content);
}
