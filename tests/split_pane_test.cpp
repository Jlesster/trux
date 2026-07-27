#include "test.hpp"

#include <cassert>
#include <trux/layout/layout.hpp>
#include <trux/layout/position.hpp>

void test_vertical_split() {
    auto root          = trux::layout::root({80, 24});
    auto [left, right] = root.v_split(25);

    assert(left.size().width == 20);
    assert(left.size().width == 20);
}

int main() { test::run("vertical_split", test_vertical_split); }
