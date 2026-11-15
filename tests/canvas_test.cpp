// Standalone test for component::Canvas and Input::set_tick_interval.
// No framework: exits non-zero on the first failed CHECK.

#include <trux/core.hpp>

#include <chrono>
#include <cstdio>
#include <cstdlib>

using namespace trux;
using component::Canvas;
using renderer::Cell;

#define CHECK(expr)                                                      \
    do {                                                                 \
        if(!(expr)) {                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__,  \
                         __LINE__, #expr);                               \
            std::exit(1);                                                \
        }                                                                \
    } while(0)

static void test_roundtrip() {
    Canvas c(4, 3);
    CHECK(c.width() == 4 && c.height() == 3);
    CHECK(c.cells().size() == 12);
    CHECK(c.contains({3, 2}) && !c.contains({4, 2}) && !c.contains({-1, 0}));

    c.set(1, 2, U'#', {1, 2, 3, 255}, {4, 5, 6, 255});
    CHECK(c.at(1, 2).glyph == U'#');
    CHECK(c.at(1, 2).foreground == (style::Color{1, 2, 3, 255}));
    CHECK(c.at(1, 2).background == (style::Color{4, 5, 6, 255}));
    CHECK(&c.cells()[2 * 4 + 1] == &c.at(1, 2));  // row-major

    c.set(99, 99, U'x');  // out of bounds is ignored
    c.set(-1, 0, U'x');

    c.fill(Cell{.glyph = U'.'});
    CHECK(c.at(0, 0).glyph == U'.' && c.at(3, 2).glyph == U'.');
    c.clear();
    CHECK(c.at(1, 2) == Cell{});

    c.set(0, 0, U'a');
    c.resize(2, 2);
    CHECK(c.size() == (layout::Size{2, 2}));
    CHECK(c.at(0, 0) == Cell{});  // resize clears
    c.resize(-5, 3);
    CHECK(c.width() == 0 && c.cells().empty());
}

static void test_build_plain() {
    auto root = layout::init({20, 6});
    renderer::Renderer r({20, 6});
    Canvas c(5, 2);
    c.set(0, 0, U'A');
    c.set(4, 1, U'Z');
    c.set(2, 1, U' ', {255, 255, 255, 255}, {9, 9, 9, 255});  // bg-only cell

    r.begin_draw();
    r.push(c, layout::Region({3, 1}, {10, 4}));
    r.end_draw();

    auto& b = r.back_buffer();
    CHECK(b.at({3, 1}).glyph == U'A');
    CHECK(b.at({7, 2}).glyph == U'Z');
    CHECK(b.at({5, 2}).background == (style::Color{9, 9, 9, 255}));
    CHECK(b.at({4, 1}).glyph == U' ');  // empty cell draws nothing
    (void)root;
}

static void test_border() {
    renderer::Renderer r({20, 6});
    Canvas c;
    c.flags.add(component::Flag::BorderRounded);
    layout::Region area({0, 0}, {8, 5});
    c.fit(area);
    CHECK(c.size() == (layout::Size{6, 3}));  // border-aware
    c.set(0, 0, U'X');
    c.set(5, 2, U'Y');

    r.begin_draw();
    r.push(c, area);
    r.end_draw();

    auto& b = r.back_buffer();
    CHECK(b.at({0, 0}).glyph == U'╭');
    CHECK(b.at({7, 4}).glyph == U'╯');
    CHECK(b.at({1, 1}).glyph == U'X');  // content inset by 1
    CHECK(b.at({6, 3}).glyph == U'Y');

    // fit() keeps contents when the size is unchanged.
    c.fit(area);
    CHECK(c.at(0, 0).glyph == U'X');
}

static void test_clipping_and_background() {
    renderer::Renderer r({20, 6});
    Canvas c(10, 10);
    c.fill(Cell{.glyph = U'#'});
    c.background = style::Color{10, 20, 30, 255};

    r.begin_draw();
    r.push(c, layout::Region({2, 1}, {4, 3}));  // much smaller than canvas
    r.end_draw();

    auto& b = r.back_buffer();
    CHECK(b.at({2, 1}).glyph == U'#');
    CHECK(b.at({5, 3}).glyph == U'#');
    CHECK(b.at({6, 1}).glyph == U' ');  // nothing leaked right...
    CHECK(b.at({2, 4}).glyph == U' ');  // ...or below
    CHECK(b.at({6, 1}).background.a == 0);

    // Canvas smaller than the region: background still fills it.
    Canvas d(1, 1);
    d.background = style::Color{1, 1, 1, 255};
    d.set(0, 0, U'q');
    r.begin_draw();
    r.push(d, layout::Region({0, 0}, {3, 2}));
    r.end_draw();
    CHECK(r.back_buffer().at({0, 0}).glyph == U'q');
    CHECK(r.back_buffer().at({2, 1}).background == (style::Color{1, 1, 1, 255}));
}

static void test_fps() {
    Canvas c;
    CHECK(c.fps() == 30 && c.frame_interval_ms() == 33);
    c.set_fps(60);
    CHECK(c.fps() == 60 && c.frame_interval_ms() == 16);
    c.set_fps(100000);
    CHECK(c.fps() == Canvas::kMaxFps);
    c.set_fps(-4);
    CHECK(c.fps() == 0 && c.frame_interval_ms() == 0);
}

static void test_tick_interval() {
    using namespace std::chrono_literals;
    input::Input in;
    CHECK(in.tick_interval() == 100ms);
    in.set_tick_interval(16ms);
    CHECK(in.tick_interval() == 16ms);
    in.set_tick_interval(0ms);  // non-positive restores the default
    CHECK(in.tick_interval() == 100ms);
}

int main() {
    test_roundtrip();
    test_build_plain();
    test_border();
    test_clipping_and_background();
    test_fps();
    test_tick_interval();
    std::puts("canvas_test: all passed");
}
