#include "test.hpp"
#include "trux/component/component.hpp"
#include "trux/component/dropdown.hpp"
#include "trux/component/label.hpp"
#include "trux/component/list.hpp"
#include "trux/component/menu.hpp"
#include "trux/input/event.hpp"
#include "trux/input/key.hpp"
#include "trux/layout/layout.hpp"
#include "trux/renderer/renderer.hpp"

#include <cassert>
#include <ranges>
#include <string>
#include <vector>

using namespace trux;

void label_test() {
    renderer::Renderer renderer({10, 5});

    auto label = component::Label("Hello");

    renderer.begin_draw();
    renderer.push(label, layout::init({10, 5}));
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

    int  list_offset = 0;
    auto list = component::List(items, list_offset) | component::BorderRounded;

    renderer.begin_draw();
    renderer.push(list, layout::init({20, 6}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({0, 0}).glyph == U'╭');
    assert(renderer.back_buffer().at({1, 1}).glyph == U'o');
    assert(renderer.back_buffer().at({1, 2}).glyph == U't');
}

void list_scroll_test() {
    renderer::Renderer       renderer({20, 6});
    std::vector<std::string> items = {
        "one", "two", "three", "four", "five", "six"};
    int  scroll_offset = 2;
    auto list =
        component::List(items, scroll_offset) | component::BorderRounded;

    renderer.begin_draw();
    renderer.push(list, layout::init({20, 6}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({1, 1}).glyph == U't');  // "three"
    assert(renderer.back_buffer().at({1, 2}).glyph == U'f');  // "four"
}

void list_scroll_clamp_test() {
    renderer::Renderer       renderer({20, 6});
    std::vector<std::string> items = {
        "one", "two", "three", "four", "five", "six"};
    int  scroll_offset = 999;  // way past the end
    auto list =
        component::List(items, scroll_offset) | component::BorderRounded;

    renderer.begin_draw();
    renderer.push(list, layout::init({20, 6}));
    renderer.end_draw();

    assert(scroll_offset == 2);
}

void assign_flag_test() {
    std::vector<std::string> items = {"a", "b"};

    int             list_offset = 0;
    component::List list(items, list_offset);

    list |= component::Italic;

    assert(list.flags.has(component::Flag::Italic));
}

void menu_test() {
    renderer::Renderer       renderer({20, 6});
    std::vector<std::string> items         = {"one", "two", "three"};
    int                      selected      = 1;
    int                      scroll_offset = 0;
    auto menu = component::Menu(items, selected, scroll_offset) |
                component::BorderRounded;

    renderer.begin_draw();
    renderer.push(menu, layout::init({20, 6}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({0, 0}).glyph == U'╭');
    assert(renderer.back_buffer().at({1, 1}).glyph == U'o');  // "one"
    assert(renderer.back_buffer().at({1, 2}).glyph ==
           U't');  // "two" (selected)
    assert(renderer.back_buffer().at({1, 2}).style == style::Style::Italic);
    assert(renderer.back_buffer().at({1, 1}).style == style::Style::None);
}

void menu_handle_down_test() {
    std::vector<std::string> items         = {"one", "two", "three"};
    int                      selected      = 0;
    int                      scroll_offset = 0;
    component::Menu          menu(items, selected, scroll_offset);

    bool consumed = menu.handle(input::Event::key(input::Key::Down));
    assert(consumed);
    assert(selected == 1);
}

void menu_handle_up_wraps_test() {
    std::vector<std::string> items         = {"one", "two", "three"};
    int                      selected      = 0;
    int                      scroll_offset = 0;
    component::Menu          menu(items, selected, scroll_offset);

    bool consumed = menu.handle(input::Event::key(input::Key::Up));
    assert(consumed);
    assert(selected == static_cast<int>(items.size()) - 1);  // wraps to last
}

void menu_handle_unhandled_key_test() {
    std::vector<std::string> items         = {"one", "two"};
    int                      selected      = 0;
    int                      scroll_offset = 0;
    component::Menu          menu(items, selected, scroll_offset);

    bool consumed = menu.handle(input::Event::key(static_cast<char32_t>('x')));
    assert(!consumed);
    assert(selected == 0);  // unchanged
}

void menu_handle_empty_items_test() {
    std::vector<std::string> items         = {};
    int                      selected      = 0;
    int                      scroll_offset = 0;
    component::Menu          menu(items, selected, scroll_offset);

    assert(!menu.handle(input::Event::key(input::Key::Down)));
}

void menu_scroll_follows_selection_test() {
    renderer::Renderer renderer({20, 6});  // content height 4 after border
    std::vector<std::string> items         = {"a", "b", "c", "d", "e", "f"};
    int                      selected      = 0;
    int                      scroll_offset = 0;
    auto menu = component::Menu(items, selected, scroll_offset) |
                component::BorderRounded;

    renderer.begin_draw();
    renderer.push(menu, layout::init({20, 6}));
    renderer.end_draw();

    for(int i = 0; i < 4; i++) {
        bool consumed = menu.handle(input::Event::key(input::Key::Down));
        assert(consumed);
    }
    assert(selected == 4);
    assert(scroll_offset ==
           1);  // bottom edge should have pushed offset forward
}

void dropdown_test() {
    renderer::Renderer       renderer({20, 5});
    std::vector<std::string> options       = {"yes", "no", "maybe"};
    int                      scroll_offset = 0;
    auto dropdown = component::Dropdown(options, scroll_offset);
    dropdown.open = true;

    renderer.begin_draw();
    renderer.push(dropdown, layout::init({20, 5}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({0, 0}).glyph == U'y');  // "yes"
    assert(renderer.back_buffer().at({0, 1}).glyph == U'n');  // "no"
    assert(renderer.back_buffer().at({0, 2}).glyph == U'm');  // "maybe"
}

void dropdown_default_state_test() {
    std::vector<std::string> options       = {"yes", "no"};
    int                      scroll_offset = 0;
    component::Dropdown      dropdown(options, scroll_offset);
    assert(dropdown.selected == 0);
    assert(dropdown.open == false);
}

void container_test() {
    renderer::Renderer       renderer({20, 4});
    std::vector<std::string> left_items   = {"L"};
    std::vector<std::string> right_items  = {"R"};
    int                      left_scroll  = 0;
    int                      right_scroll = 0;

    component::Container container;
    container.percent    = 50;
    container.horizontal = true;
    container.first =
        std::make_unique<component::ComponentWrapper<component::List>>(
            component::List(left_items, left_scroll));
    container.second =
        std::make_unique<component::ComponentWrapper<component::List>>(
            component::List(right_items, right_scroll));

    renderer::DrawCommandBuffer cmd;
    container.build(layout::init({20, 4}), cmd);

    assert(!cmd.commands().empty());
}

int main() {
    test::run("label renders text", label_test);
    test::run("label flag modifier", label_flag_test);
    test::run("list with border renders", list_test);
    test::run("assign flag modifier", assign_flag_test);
    test::run("list scroll shows offset window", list_scroll_test);
    test::run("list scroll offset clamps to valid range",
              list_scroll_clamp_test);

    test::run("menu renders with selection styled", menu_test);
    test::run("menu handles down key", menu_handle_down_test);
    test::run("menu handles up key wraps", menu_handle_up_wraps_test);
    test::run("menu ignores unhandled key", menu_handle_unhandled_key_test);
    test::run("menu handles empty items safely", menu_handle_empty_items_test);
    test::run("menu scroll follows selection past bottom edge",
              menu_scroll_follows_selection_test);

    test::run("dropdown renders options", dropdown_test);
    test::run("dropdown default state", dropdown_default_state_test);

    test::run("container builds both children", container_test);

    return test::summary();
}
