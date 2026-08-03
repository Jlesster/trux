#include "test.hpp"
#include "trux/component/component.hpp"
#include "trux/component/label.hpp"
#include "trux/component/list.hpp"
#include "trux/layout/layout.hpp"
#include "trux/renderer/renderer.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace trux;

void label_test() {
    renderer::Renderer renderer({10, 5});

    renderer.begin_draw();
    renderer.push(component::Label("Hello"), layout::init({10, 5}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({0, 0}).glyph == U'H');
    assert(renderer.back_buffer().at({4, 0}).glyph == U'o');
}

void label_flag_test() {
    auto label = component::Label("Hi") | component::Bold;

    assert(label.flags.has(component::Flag::Bold));
}

void list_test() {
    renderer::Renderer renderer({20, 6});

    std::vector<std::string> items = {"one", "two"};
    auto list = component::List(items) | component::BorderRounded;

    renderer.begin_draw();
    renderer.push(list, layout::init({20, 6}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({0, 0}).glyph == U'╭');
    assert(renderer.back_buffer().at({1, 1}).glyph == U'o');
    assert(renderer.back_buffer().at({1, 2}).glyph == U't');
}

void assign_flag_test() {
    std::vector<std::string> items = {"a", "b"};
    component::List          list(items);

    list |= component::Italic;

    assert(list.flags.has(component::Flag::Italic));
}

int main() {
    test::run("label renders text", label_test);
    test::run("label flag modifier", label_flag_test);
    test::run("list with border renders", list_test);
    test::run("assign flag modifier", assign_flag_test);
}
