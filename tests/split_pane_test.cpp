#include "test.hpp"

#include <cassert>
#include <string>
#include <trux/component/canvas.hpp>
#include <trux/component/menu.hpp>
#include <trux/component/split.hpp>
#include <trux/input/event.hpp>
#include <trux/input/key.hpp>
#include <trux/layout/layout.hpp>
#include <trux/layout/position.hpp>
#include <vector>

using namespace trux;

void test_vertical_split() {
    auto root          = trux::layout::init({80, 24});
    auto [left, right] = root.v_split(25);

    assert(left.size().width == 20);
    assert(left.size().width == 20);
}

// A Canvas is display-only, so component::Split's focus cycling must skip
// it and never make it the active pane.
void test_focus_skips_non_focusable_leaf() {
    std::vector<std::string> items = {"one", "two"};
    component::Canvas canvas(4, 3);
    component::Menu menu(items);

    component::Split panes(canvas);
    panes.split_v(menu);  // the focusable pane becomes active

    // Only the Menu is focusable, so cycling is a no-op and the Menu stays
    // active; a Down key therefore reaches it (the Canvas would decline).
    panes.focus_next();
    assert(panes.handle(input::Event::key(input::Key::Down)));
}

// Splitting in a non-focusable pane must not steal focus from the active one.
void test_new_non_focusable_leaf_keeps_active() {
    std::vector<std::string> items = {"one", "two"};
    component::Canvas canvas(4, 3);
    component::Menu menu(items);

    component::Split panes(menu);
    panes.split_v(canvas);  // new pane is non-focusable

    assert(panes.handle(input::Event::key(input::Key::Down)));
}

int main() {
    test::run("vertical_split", test_vertical_split);
    test::run("focus skips non-focusable leaf",
              test_focus_skips_non_focusable_leaf);
    test::run("new non-focusable leaf keeps active",
              test_new_non_focusable_leaf_keeps_active);
    return test::summary();
}
