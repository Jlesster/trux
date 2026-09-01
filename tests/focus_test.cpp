#include "test.hpp"

#include <cassert>
#include <trux/renderer/renderer.hpp>

using namespace trux;

namespace {
struct FakeWidget {
    component::ComponentFlags flags{};
    int                       calls{0};
    void build(layout::Region, renderer::DrawCommandBuffer&) const {}
    bool handle(const input::Event&) {
        calls++;
        return true;
    }
};
}  // namespace

void test_first_pushed_is_focused_by_default() {
    renderer::Renderer r({80, 24});
    FakeWidget         a, b;

    r.begin_draw();
    r.push(a,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.push(b,
           layout::Region{
               {0,  10},
               {10, 10}
    });
    r.end_draw();

    bool consumed = r.dispatch(input::Event::key(U'x'));
    assert(consumed);
    assert(a.calls == 1);
    assert(b.calls == 0);
}

void test_tab_cycles_focus_forward() {
    renderer::Renderer r({80, 24});
    FakeWidget         a, b;

    r.begin_draw();
    r.push(a,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.push(b,
           layout::Region{
               {0,  10},
               {10, 10}
    });
    r.end_draw();

    bool tab_consumed = r.dispatch(input::Event::key(input::Key::Tab));
    assert(tab_consumed);  // Tab is always consumed by focus traversal

    bool x_consumed = r.dispatch(input::Event::key(U'x'));
    assert(x_consumed);
    assert(a.calls == 0);
    assert(b.calls == 1);
}

void test_shift_tab_cycles_backward() {
    renderer::Renderer r({80, 24});
    FakeWidget         a, b;

    r.begin_draw();
    r.push(a,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.push(b,
           layout::Region{
               {0,  10},
               {10, 10}
    });
    r.end_draw();

    input::Modifiers shift{};
    shift.add(input::Mod::Shift);

    bool tab_consumed = r.dispatch(input::Event::key(input::Key::Tab, shift));
    assert(tab_consumed);

    bool x_consumed = r.dispatch(input::Event::key(U'x'));
    assert(x_consumed);
    assert(b.calls == 1);
    assert(a.calls == 0);
}

void test_refocus_when_focused_widget_not_pushed_next_frame() {
    renderer::Renderer r({80, 24});
    FakeWidget         a, b;

    r.begin_draw();
    r.push(a,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.push(b,
           layout::Region{
               {0,  10},
               {10, 10}
    });
    r.end_draw();

    bool tab_consumed =
        r.dispatch(input::Event::key(input::Key::Tab));  // focus moves to b
    assert(tab_consumed);

    // next frame: b's pane is gone
    r.begin_draw();
    r.push(a,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.end_draw();

    bool x_consumed = r.dispatch(input::Event::key(U'x'));
    assert(x_consumed);
    assert(a.calls == 1);
}

void test_mouse_click_focuses_component_under_cursor() {
    renderer::Renderer r({80, 24});
    FakeWidget         a, b;

    r.begin_draw();
    r.push(a,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.push(b,
           layout::Region{
               {0,  10},
               {10, 10}
    });
    r.end_draw();

    input::MouseEvent click{
        .position = {2, 12},
        .button   = input::MouseButton::Left,
        .kind     = input::MouseKind::Press
    };
    bool consumed = r.dispatch(input::Event::from_mouse(click));
    assert(consumed);
    assert(b.calls == 1);
    assert(a.calls == 0);

    bool key_consumed = r.dispatch(input::Event::key(U'x'));
    assert(key_consumed);
    assert(b.calls == 2);
}

void test_mouse_click_outside_any_region_leaves_focus_unchanged() {
    renderer::Renderer r({80, 24});
    FakeWidget         a, b;

    r.begin_draw();
    r.push(a,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.push(b,
           layout::Region{
               {0,  10},
               {10, 10}
    });
    r.end_draw();

    input::MouseEvent click{
        .position = {50, 50},
        .button   = input::MouseButton::Left,
        .kind     = input::MouseKind::Press
    };
    bool consumed = r.dispatch(input::Event::from_mouse(click));
    assert(consumed);
    assert(a.calls == 1);
    assert(b.calls == 0);
}

void test_mouse_click_overlap_prefers_last_pushed() {
    renderer::Renderer r({80, 24});
    FakeWidget         a, b;

    r.begin_draw();
    r.push(a,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.push(b,
           layout::Region{
               {0,  0 },
               {10, 10}
    });
    r.end_draw();

    input::MouseEvent click{
        .position = {5, 5},
        .button   = input::MouseButton::Left,
        .kind     = input::MouseKind::Press
    };
    bool consumed = r.dispatch(input::Event::from_mouse(click));
    assert(consumed);
    assert(b.calls == 1);
    assert(a.calls == 0);
}

int main() {
    test::run("first_pushed_is_focused_by_default",
              test_first_pushed_is_focused_by_default);
    test::run("tab_cycles_focus_forward", test_tab_cycles_focus_forward);
    test::run("shift_tab_cycles_backward", test_shift_tab_cycles_backward);
    test::run("refocus_when_focused_widget_not_pushed_next_frame",
              test_refocus_when_focused_widget_not_pushed_next_frame);
    test::run("mouse_click_focuses_component_under_cursor",
              test_mouse_click_focuses_component_under_cursor);
    test::run("mouse_click_outside_region_loses_focus",
              test_mouse_click_outside_any_region_leaves_focus_unchanged);
    test::run("mouse_click_overlap_prefers_last_pushed",
              test_mouse_click_overlap_prefers_last_pushed);
    return test::summary();
}
