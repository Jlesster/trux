#include "test.hpp"
#include "trux/input/key.hpp"
#include "trux/layout/layout.hpp"
#include "trux/renderer/renderer.hpp"

#include <cassert>
#include <trux/widget/label.hpp>

using namespace trux;

void label_test() {
    renderer::Renderer renderer({10, 5});

    widget::Label label("Hello");
    label.draw(renderer, layout::init({10, 5}));

    assert(renderer.back_buffer().at({0, 0}).glyph == U'H');
}

void label_input_test() {
    widget::Label label("Hello");

    input::Event event{input::KeyEvent{.key = input::Key::Enter}};

    assert(!label.handle(event));
}

int main() {
    test::run("label test", label_test);
    test::run("label input test", label_input_test);
}
