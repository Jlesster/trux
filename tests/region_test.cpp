#include "test.hpp"
#include "trux/layout/position.hpp"
#include "trux/renderer/renderer.hpp"

#include <cassert>
#include <trux/layout/region.hpp>

using namespace trux;

void clipping_test() {
    layout::Region region({10, 10}, {20, 20});

    assert(region.contains({10, 10}));
    assert(region.contains({29, 29}));

    assert(!region.contains({30, 30}));
    assert(!region.contains({9, 10}));
}

void test_region_translation_test() {
    auto region = layout::Region{
        {10, 10},
        {20, 20}
    };

    auto origin = layout::Position{10, 10};
    auto offset = layout::Position{15, 15};

    assert(region.absolute({0, 0}) == origin);
    assert(region.absolute({5, 5}) == offset);
}

void test_region_reflects_size_after_resize() {
    renderer::Renderer r({80, 24});
    r.begin_draw();
    assert(r.region().size().width == 80);
    assert(r.region().size().height == 24);
    r.end_draw();

    r.resize({100, 30});
    r.begin_draw();
    assert(r.region().size().width == 100);
    assert(r.region().size().height == 30);
    r.end_draw();
}

int main() {
    test::run("clipping test", clipping_test);
    test::run("region_translation test", test_region_translation_test);
    test::run("region_reflects_size_after_resize test",
              test_region_reflects_size_after_resize);
    return test::summary();
}
