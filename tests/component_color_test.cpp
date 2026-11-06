#include "test.hpp"
#include "trux/component/checkbox.hpp"
#include "trux/component/component.hpp"
#include "trux/component/label.hpp"
#include "trux/component/menu.hpp"
#include "trux/layout/layout.hpp"
#include "trux/renderer/renderer.hpp"

#include <cassert>
#include <string>
#include <vector>

using namespace trux;
namespace c = trux::component;

void color_index_overload_test() {
    auto m = c::Menu() | c::Color(AnsiColor::GREEN);
    assert(m.text_color && m.text_color->is_indexed());
    assert(m.text_color->index == 2);
    assert(m.selection_color == m.text_color);
}

void color_braced_rgb_overload_test() {
    int r = 160, g = 0, b = 160;
    auto m = c::Menu() | c::Color({160, 0, 160});
    assert(m.text_color->is_rgb() && m.text_color->r == 160);

    auto v = c::Menu() | c::Color({r, g, b});  // int variables, no narrowing
    assert(v.text_color->is_rgb() && v.text_color->b == 160);

    auto a = c::Menu() | c::Color({1, 2, 3, 200});
    assert(a.text_color->a == 200);
}

void color_triple_overload_and_clamp_test() {
    auto m = c::Menu() | c::Color(10, 20, 999);
    assert(m.text_color->g == 20 && m.text_color->b == 255);
    auto n = c::Menu() | c::Color(AnsiColor::BLACK);
    assert(n.text_color->index == 0);
}

void color_composes_with_flags_test() {
    c::Menu m;
    m |= c::BorderRounded | c::Color({160, 0, 160});
    assert(m.flags.has(c::Flag::BorderRounded));
    assert(m.text_color && m.text_color->is_rgb());

    auto both = c::Color(AnsiColor::YELLOW) | c::Bold;
    auto l = c::Label("x") | both;
    assert(l.flags.has(c::Flag::Bold) && l.text_color->index == 3);
}

void color_skips_missing_fields_test() {
    // Label has text_color but no selection_color; must still compile.
    auto l = c::Label("x") | c::Color(AnsiColor::RED);
    assert(l.text_color && l.text_color->index == 1);
}

void color_unset_keeps_default_test() {
    renderer::Renderer renderer({10, 3});
    std::vector<std::string> items{"abc"};
    auto menu = c::Menu(items);
    renderer.begin_draw();
    renderer.push(menu, layout::init({10, 3}));
    renderer.end_draw();
    assert(renderer.back_buffer().at({0, 0}).foreground ==
           style::Color(255, 255, 255, 255));
}

void color_reaches_cells_test() {
    renderer::Renderer renderer({10, 3});
    std::vector<std::string> items{"abc", "def"};
    auto menu = c::Menu(items) | c::Color(AnsiColor::GREEN);
    menu.selection_color = style::Color(10, 20, 30);

    renderer.begin_draw();
    renderer.push(menu, layout::init({10, 3}));
    renderer.end_draw();

    auto& buf = renderer.back_buffer();
    assert(buf.at({0, 0}).foreground == style::Color::Ansi(2) ||
           buf.at({0, 0}).foreground == style::Color(10, 20, 30));
    // Row 0 is selected -> selection_color; row 1 -> text_color.
    assert(buf.at({0, 0}).foreground == style::Color(10, 20, 30));
    assert(buf.at({0, 1}).foreground == style::Color::Ansi(2));
}

void color_checkbox_glyph_test() {
    renderer::Renderer renderer({10, 2});
    std::vector<std::string> items{"abc"};
    std::vector<bool> checked{false};
    auto cb = c::Checkbox(items, checked) | c::Color({1, 2, 3});
    renderer.begin_draw();
    renderer.push(cb, layout::init({10, 2}));
    renderer.end_draw();
    assert(renderer.back_buffer().at({0, 0}).foreground ==
           style::Color(1, 2, 3));
    assert(renderer.back_buffer().at({4, 0}).foreground ==
           style::Color(1, 2, 3));
}

void border_color_survives_focus_test() {
    renderer::Renderer renderer({10, 4});
    std::vector<std::string> items{"abc"};
    auto menu = c::Menu(items) | c::BorderRounded | c::Color(AnsiColor::RED) |
                c::BorderColor({1, 2, 3});
    assert(menu.border_color == style::Color(1, 2, 3));
    assert(menu.text_color->index == 1);  // BorderColor leaves text alone

    renderer.begin_draw();
    renderer.push(menu, layout::init({10, 4}));
    renderer.end_draw();
    assert(menu.border_color == style::Color(1, 2, 3));
    assert(renderer.back_buffer().at({0, 0}).foreground ==
           style::Color(1, 2, 3));
}

void border_color_index_test() {
    auto m = c::Menu() | c::BorderColor(AnsiColor::RED);
    assert(m.border_color == style::Color::Ansi(1));
    assert(m.border_color_fixed == style::Color::Ansi(1));
    assert(m.border_color_active == style::Color::Ansi(1));
    assert(!m.text_color);
}

void border_color_pair_test() {
    auto ansi = c::Menu() | c::BorderColor(AnsiColor::RED, AnsiColor::GREEN);
    assert(ansi.border_color_fixed == style::Color::Ansi(1));
    assert(ansi.border_color_active == style::Color::Ansi(2));

    auto rgb = c::Menu() | c::BorderColor({1, 2, 3}, {4, 5, 6});
    assert(rgb.border_color_fixed == style::Color(1, 2, 3));
    assert(rgb.border_color_active == style::Color(4, 5, 6));
}

void border_color_focus_render_test() {
    renderer::Renderer renderer({10, 6});
    std::vector<std::string> items{"abc"};

    auto a = c::Menu(items) | c::BorderRounded |
             c::BorderColor(AnsiColor::RED, AnsiColor::GREEN);
    auto b = c::Menu(items) | c::BorderRounded |
             c::BorderColor(AnsiColor::RED, AnsiColor::GREEN);

    layout::Region ra{{0, 0}, {10, 3}};
    layout::Region rb{{0, 3}, {10, 3}};

    renderer.begin_draw();
    renderer.push(a, ra);
    renderer.push(b, rb);
    renderer.end_draw();

    // First pushed menu is focused -> active (GREEN); second -> fixed (RED).
    assert(renderer.back_buffer().at({0, 0}).foreground ==
           style::Color::Ansi(2));
    assert(renderer.back_buffer().at({0, 3}).foreground ==
           style::Color::Ansi(1));

    assert(renderer.dispatch(input::Event::key(input::Key::Tab)));

    renderer.begin_draw();
    renderer.push(a, ra);
    renderer.push(b, rb);
    renderer.end_draw();

    // Focus swapped -> colors swap.
    assert(renderer.back_buffer().at({0, 0}).foreground ==
           style::Color::Ansi(1));
    assert(renderer.back_buffer().at({0, 3}).foreground ==
           style::Color::Ansi(2));
}

int main() {
    test::run("BorderColor survives focus recolor",
              border_color_survives_focus_test);
    test::run("BorderColor(int) -> indexed", border_color_index_test);
    test::run("BorderColor pair -> fixed/active", border_color_pair_test);
    test::run("BorderColor active follows focus",
              border_color_focus_render_test);
    test::run("Color(int) -> indexed", color_index_overload_test);
    test::run("Color({r,g,b}) -> truecolor", color_braced_rgb_overload_test);
    test::run("Color(r,g,b) and clamping",
              color_triple_overload_and_clamp_test);
    test::run("Color composes with flag modifiers",
              color_composes_with_flags_test);
    test::run("Color skips components without a field",
              color_skips_missing_fields_test);
    test::run("unset color keeps default fg", color_unset_keeps_default_test);
    test::run("Color reaches rendered cells", color_reaches_cells_test);
    test::run("Checkbox glyph + label colored", color_checkbox_glyph_test);
    return test::summary();
}
