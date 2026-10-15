#include "test.hpp"
#include "trux/component/checkbox.hpp"
#include "trux/component/component.hpp"
#include "trux/component/dialog.hpp"
#include "trux/component/dropdown.hpp"
#include "trux/component/label.hpp"
#include "trux/component/list.hpp"
#include "trux/component/menu.hpp"
#include "trux/component/paragraph.hpp"
#include "trux/component/split.hpp"
#include "trux/component/text_input.hpp"
#include "trux/input/event.hpp"
#include "trux/input/key.hpp"
#include "trux/layout/layout.hpp"
#include "trux/renderer/renderer.hpp"

#include <cassert>
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

    auto list = component::List(items) | component::BorderRounded;

    renderer.begin_draw();
    renderer.push(list, layout::init({20, 6}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({0, 0}).glyph == U'╭');
    assert(renderer.back_buffer().at({1, 1}).glyph == U'o');
    assert(renderer.back_buffer().at({1, 2}).glyph == U't');
}

void list_scroll_test() {
    renderer::Renderer renderer({20, 6});
    std::vector<std::string> items = {
        "one", "two", "three", "four", "five", "six"};
    auto list = component::List(items) | component::BorderRounded;
    list.offset = 2;

    renderer.begin_draw();
    renderer.push(list, layout::init({20, 6}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({1, 1}).glyph == U't');  // "three"
    assert(renderer.back_buffer().at({1, 2}).glyph == U'f');  // "four"
}

void list_scroll_clamp_test() {
    renderer::Renderer renderer({20, 6});
    std::vector<std::string> items = {
        "one", "two", "three", "four", "five", "six"};
    auto list = component::List(items) | component::BorderRounded;
    list.offset = 999;  // way past the end

    renderer.begin_draw();
    renderer.push(list, layout::init({20, 6}));
    renderer.end_draw();

    assert(list.offset == 2);
}

void assign_flag_test() {
    std::vector<std::string> items = {"a", "b"};

    component::List list(items);

    list |= component::Italic;

    assert(list.flags.has(component::Flag::Italic));
}

void menu_test() {
    renderer::Renderer renderer({20, 6});
    std::vector<std::string> items = {"one", "two", "three"};
    auto menu = component::Menu(items) | component::BorderRounded;
    menu.selection = 1;

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
    std::vector<std::string> items = {"one", "two", "three"};
    component::Menu menu(items);

    bool consumed = menu.handle(input::Event::key(input::Key::Down));
    assert(consumed);
    assert(menu.selection == 1);
}

void menu_handle_up_wraps_test() {
    std::vector<std::string> items = {"one", "two", "three"};
    component::Menu menu(items);

    bool consumed = menu.handle(input::Event::key(input::Key::Up));
    assert(consumed);
    assert(menu.selection ==
           static_cast<int>(items.size()) - 1);  // wraps to last
}

void menu_handle_unhandled_key_test() {
    std::vector<std::string> items = {"one", "two"};
    component::Menu menu(items);

    bool consumed = menu.handle(input::Event::key(static_cast<char32_t>('x')));
    assert(!consumed);
    assert(menu.selection == 0);  // unchanged
}

void menu_handle_empty_items_test() {
    std::vector<std::string> items = {};
    component::Menu menu(items);

    assert(!menu.handle(input::Event::key(input::Key::Down)));
}

void menu_scroll_follows_selection_test() {
    renderer::Renderer renderer({20, 6});  // content height 4 after border
    std::vector<std::string> items = {"a", "b", "c", "d", "e", "f"};
    auto menu = component::Menu(items) | component::BorderRounded;

    renderer.begin_draw();
    renderer.push(menu, layout::init({20, 6}));
    renderer.end_draw();

    for(int i = 0; i < 4; i++) {
        bool consumed = menu.handle(input::Event::key(input::Key::Down));
        assert(consumed);
    }
    assert(menu.selection == 4);
    assert(menu.offset == 1);  // bottom edge should have pushed offset forward
}

void menu_selection_clamps_when_items_shrink_test() {
    renderer::Renderer renderer({20, 6});
    std::vector<std::string> items = {"a", "b", "c"};
    auto menu = component::Menu(items) | component::BorderRounded;
    menu.selection = 2;

    items.pop_back();  // selection now points one past the end
    items.pop_back();
    renderer.begin_draw();
    renderer.push(menu, layout::init({20, 6}));
    renderer.end_draw();
    assert(menu.selection == 0);  // build() clamped to the last item

    items = {"a", "b", "c"};
    menu.selection = 2;
    items.resize(1);
    // handle() must clamp before wrapping, without a build in between
    assert(menu.handle(input::Event::key(input::Key::Down)));
    assert(menu.selection == 0);
}

void menu_selection_clamps_when_emptied_test() {
    std::vector<std::string> items = {"a", "b"};
    component::Menu menu(items);
    menu.selection = 1;
    items.clear();
    assert(!menu.handle(input::Event::key(input::Key::Down)));
    menu.clamp();
    assert(menu.selection == 0);

    items = {"x", "y", "z"};  // grows again: selection stays valid
    assert(menu.selection == 0);
}

void checkbox_cursor_clamps_when_items_shrink_test() {
    std::vector<std::string> items = {"a", "b", "c"};
    std::vector<bool> checked(3, false);
    component::Checkbox box(items, checked);
    box.cursor = 2;
    items.resize(1);
    checked.resize(1);
    assert(box.handle(input::Event::key(input::Key::Down)));
    assert(box.cursor == 0);
}

void dropdown_selection_clamps_when_options_shrink_test() {
    std::vector<std::string> options = {"a", "b", "c"};
    component::Dropdown dd(options);
    dd.selection = 2;
    dd.open = true;
    options.resize(1);
    assert(dd.handle(input::Event::key(input::Key::Down)));
    assert(dd.selection == 0);
}

void dropdown_test() {
    renderer::Renderer renderer({20, 5});
    std::vector<std::string> options = {"yes", "no", "maybe"};
    auto dropdown = component::Dropdown(options);
    dropdown.open = true;

    renderer.begin_draw();
    renderer.push(dropdown, layout::init({20, 5}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({0, 0}).glyph == U'y');  // "yes"
    assert(renderer.back_buffer().at({0, 1}).glyph == U'n');  // "no"
    assert(renderer.back_buffer().at({0, 2}).glyph == U'm');  // "maybe"
}

void dropdown_default_state_test() {
    std::vector<std::string> options = {"yes", "no"};
    component::Dropdown dropdown(options);
    assert(dropdown.selection == 0);
    assert(dropdown.open == false);
}

void container_test() {
    renderer::Renderer renderer({20, 4});
    std::vector<std::string> left_items = {"L"};
    std::vector<std::string> right_items = {"R"};

    using StringList = component::List;

    component::Container container;
    container.percent = 50;
    container.horizontal = true;
    container.first = std::make_unique<component::ComponentWrapper<StringList>>(
        StringList(left_items));
    container.second =
        std::make_unique<component::ComponentWrapper<StringList>>(
            StringList(right_items));

    renderer::DrawCommandBuffer cmd;
    container.build(layout::init({20, 4}), cmd);

    assert(!cmd.commands().empty());
}

void menu_deferred_bind_test() {
    // A Menu can live in a struct unbound and be assigned later.
    struct Ui {
        component::Menu list;
    } ui;

    assert(ui.list.size() == 0);
    assert(!ui.list.handle(input::Event::key(input::Key::Down)));

    std::vector<std::string> items = {"one", "two", "three"};
    ui.list = component::Menu(items) | component::BorderRounded;
    ui.list.selection = 2;

    assert(ui.list.size() == 3);
    assert(ui.list.selection == 2);
    assert(ui.list.handle(input::Event::key(input::Key::Down)));
    assert(ui.list.selection == 0);  // wrapped
}

void menu_unbound_build_test() {
    renderer::Renderer renderer({20, 6});
    component::Menu menu;
    menu |= component::BorderRounded;

    renderer.begin_draw();
    renderer.push(menu, layout::init({20, 6}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({0, 0}).glyph == U'╭');
    assert(renderer.back_buffer().at({1, 1}).glyph == U' ');
}

struct Language {
    std::string name;
};

void menu_projection_test() {
    renderer::Renderer renderer({20, 6});
    std::vector<Language> langs = {{"c++"}, {"rust"}};
    auto menu =
        component::Menu(langs, &Language::name) | component::BorderRounded;

    renderer.begin_draw();
    renderer.push(menu, layout::init({20, 6}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({1, 1}).glyph == U'c');
    assert(renderer.back_buffer().at({1, 2}).glyph == U'r');
}

void menu_set_style_beyond_size_test() {
    component::Menu menu;  // unbound; set_style must not overrun
    menu.set_style(3, style::Style::Bold);
    assert(menu.style_for(3) == style::Style::Bold);
    assert(menu.style_for(0) == style::Style::None);
}

void checkbox_toggle_test() {
    std::vector<std::string> items = {"-Wall", "-O3"};
    std::vector<bool> checked = {false, false};
    component::Checkbox box(items, checked);

    assert(box.handle(input::Event::key(static_cast<char32_t>(' '))));
    assert(checked[0] && !checked[1]);

    assert(box.handle(input::Event::key(input::Key::Down)));
    assert(box.cursor == 1);
    assert(box.handle(input::Event::key(static_cast<char32_t>(' '))));
    assert(checked[1]);
}

void checkbox_render_test() {
    renderer::Renderer renderer({20, 4});
    std::vector<std::string> items = {"a", "b"};
    std::vector<bool> checked = {true, false};
    component::Checkbox box(items, checked);

    renderer.begin_draw();
    renderer.push(box, layout::init({20, 4}));
    renderer.end_draw();

    assert(renderer.back_buffer().at({1, 0}).glyph == U'x');  // "[x] a"
    assert(renderer.back_buffer().at({4, 0}).glyph == U'a');
    assert(renderer.back_buffer().at({1, 1}).glyph == U' ');  // "[ ] b"
}

void checkbox_unbound_test() {
    component::Checkbox box;
    assert(!box.handle(input::Event::key(static_cast<char32_t>(' '))));
}

void dropdown_open_and_select_test() {
    std::vector<std::string> options = {"yes", "no", "maybe"};
    component::Dropdown dropdown(options);

    assert(dropdown.handle(input::Event::key(input::Key::Enter)));
    assert(dropdown.open);
    assert(dropdown.handle(input::Event::key(input::Key::Down)));
    assert(dropdown.selection == 1);
    assert(dropdown.handle(input::Event::key(input::Key::Escape)));
    assert(!dropdown.open);
    assert(dropdown.selection == 1);
}

void text_input_owns_value_test() {
    component::TextInput input{"ab"};
    assert(input.value == "ab");
    assert(input.cursor == 2);

    assert(input.handle(input::Event::key(static_cast<char32_t>('c'))));
    assert(input.value == "abc");

    assert(input.handle(input::Event::key(input::Key::Backspace)));
    assert(input.value == "ab");

    input.set_value("xyz");
    assert(input.value == "xyz");
    assert(input.cursor == 3);
}

void dialog_confirm_test() {
    component::Dialog dialog{.title = "Rename"};
    std::string confirmed;
    dialog.on_confirm = [&] { confirmed = dialog.input.value; };
    dialog.input.set_value("main.cpp");
    dialog.open = true;

    assert(dialog.handle(input::Event::key(static_cast<char32_t>('!'))));
    assert(dialog.handle(input::Event::key(input::Key::Enter)));
    assert(confirmed == "main.cpp!");
    assert(!dialog.open);
}

void dialog_escape_test() {
    component::Dialog dialog;
    bool confirmed = false;
    dialog.on_confirm = [&] { confirmed = true; };
    dialog.open = true;

    assert(dialog.handle(input::Event::key(input::Key::Escape)));
    assert(!dialog.open);
    assert(!confirmed);
}

void dialog_render_test() {
    renderer::Renderer renderer({30, 10});
    component::Dialog dialog{.title = "Hi"};
    dialog.size = {10, 5};
    dialog |= component::BorderRounded;
    dialog.input.set_value("x");
    dialog.open = true;

    renderer.begin_draw();
    renderer.push(dialog, layout::init({30, 10}), /*modal=*/true);
    renderer.end_draw();

    // centered 10x5 box in 30x10: top-left corner at (10, 2)
    assert(renderer.back_buffer().at({10, 2}).glyph == U'╭');
    assert(renderer.back_buffer().at({11, 3}).glyph == U'H');  // title
    assert(renderer.back_buffer().at({11, 4}).glyph == U'x');  // input
}

// Every component must be default-constructible so a struct of them can
// be declared up front and bound/assigned later.
struct UiComponents {
    component::Label title;
    component::Menu menu;
    component::List log;
    component::Checkbox flags;
    component::Dropdown build_type;
    component::TextInput name;
    component::Paragraph notes;
    component::Dialog rename;
    component::Split panes;
};

void struct_of_components_test() {
    UiComponents ui;  // must compile: no implicitly-deleted default ctor

    std::vector<std::string> items = {"a", "b"};
    std::vector<bool> checked = {false, false};

    ui.title.text = "hello";
    ui.menu = component::Menu(items) | component::BorderRounded;
    ui.log = component::List(items);
    ui.flags = component::Checkbox(items, checked);
    ui.build_type = component::Dropdown(items);
    ui.name.set_value("x");
    ui.notes.text = "some words here";
    ui.rename.title = "Rename";
    ui.panes = component::Split(component::Menu(items));

    assert(ui.menu.size() == 2);
    assert(!ui.panes.empty());
}

void label_owns_text_test() {
    renderer::Renderer renderer({10, 2});
    component::Label label;
    {
        std::string temp = "tmp";
        label.text = temp;  // copied; temp may die
    }
    renderer.begin_draw();
    renderer.push(label, layout::init({10, 2}));
    renderer.end_draw();
    assert(renderer.back_buffer().at({0, 0}).glyph == U't');
}

void paragraph_wrap_and_scroll_test() {
    renderer::Renderer renderer({6, 3});
    component::Paragraph para{"aa bb cc dd"};  // wraps to "aa bb","cc dd"

    renderer.begin_draw();
    renderer.push(para, layout::init({6, 3}));
    renderer.end_draw();
    assert(renderer.back_buffer().at({0, 0}).glyph == U'a');
    assert(renderer.back_buffer().at({0, 1}).glyph == U'c');

    assert(para.handle(input::Event::key(input::Key::Down)));
    assert(para.offset == 1);
    assert(para.handle(input::Event::key(input::Key::Up)));
    assert(para.offset == 0);
}

void split_empty_safe_test() {
    component::Split panes;
    renderer::DrawCommandBuffer cmd;
    assert(panes.empty());
    panes.build(layout::init({20, 4}), cmd);
    assert(cmd.commands().empty());
    assert(!panes.handle(input::Event::key(input::Key::Down)));
    panes.focus_next();
    panes.close_active();

    std::vector<std::string> items = {"a"};
    panes.split_v(component::Menu(items));  // seeds an empty Split's root
    assert(!panes.empty());
    panes.build(layout::init({20, 4}), cmd);
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

    test::run("menu selection clamps when items shrink",
              menu_selection_clamps_when_items_shrink_test);
    test::run("menu selection clamps when emptied",
              menu_selection_clamps_when_emptied_test);
    test::run("checkbox cursor clamps when items shrink",
              checkbox_cursor_clamps_when_items_shrink_test);
    test::run("dropdown selection clamps when options shrink",
              dropdown_selection_clamps_when_options_shrink_test);

    test::run("dropdown renders options", dropdown_test);
    test::run("dropdown default state", dropdown_default_state_test);

    test::run("menu deferred bind in struct member", menu_deferred_bind_test);
    test::run("menu unbound builds border only", menu_unbound_build_test);
    test::run("menu projection", menu_projection_test);
    test::run("menu set_style past size", menu_set_style_beyond_size_test);

    test::run("checkbox toggles with space", checkbox_toggle_test);
    test::run("checkbox renders glyphs", checkbox_render_test);
    test::run("checkbox unbound ignores input", checkbox_unbound_test);

    test::run("dropdown open/select/close", dropdown_open_and_select_test);

    test::run("text input owns value", text_input_owns_value_test);
    test::run("dialog enter confirms and closes", dialog_confirm_test);
    test::run("dialog escape closes without confirm", dialog_escape_test);
    test::run("dialog renders title and input", dialog_render_test);

    test::run("struct of default-constructed components",
              struct_of_components_test);
    test::run("label owns its text", label_owns_text_test);
    test::run("paragraph wraps and scrolls", paragraph_wrap_and_scroll_test);
    test::run("empty split is safe and seedable", split_empty_safe_test);

    test::run("container builds both children", container_test);

    return test::summary();
}
