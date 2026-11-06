#include "test.hpp"

#include <cassert>
#include <trux/renderer/cell.hpp>
#include <trux/util/util.hpp>

using namespace trux;
using style::Color;
using style::ColorKind;

namespace {

renderer::Cell cell_fg(Color fg) {
    renderer::Cell c;
    c.foreground = fg;
    return c;
}

renderer::Cell cell_bg(Color bg) {
    renderer::Cell c;
    c.background = bg;
    return c;
}

}  // namespace

void test_rgb_ctor_is_truecolor() {
    Color c{10, 20, 30};
    assert(c.is_rgb() && c.r == 10 && c.g == 20 && c.b == 30 && c.a == 255);
}

void test_ansi_ctor_is_indexed() {
    assert(Color::Ansi(9).is_indexed());
    assert(Color::Ansi(9).index == 9);
    assert(Color(42) == Color::Ansi(42));
}

void test_default_ctor_and_kind_equality() {
    assert(Color::Default().is_default());
    assert(Color::Default() != Color(0, 0, 0, 255));
    assert(Color::Ansi(1) != Color::Ansi(2));
    assert(Color::Ansi(1) != Color(0, 0, 0, 255));
}

void test_sgr_rgb_unchanged() {
    // Byte-for-byte what sgr_codes produced before palette support.
    assert(util::sgr_codes(cell_fg(Color(255, 255, 255, 255))) ==
           "0;38;2;255;255;255");
    renderer::Cell c;
    c.foreground = {1, 2, 3, 255};
    c.background = {4, 5, 6, 255};
    c.style = style::Style::Bold;
    assert(util::sgr_codes(c) == "0;38;2;1;2;3;48;2;4;5;6;1");
}

void test_sgr_ansi_16() {
    assert(util::sgr_codes(cell_fg(Color::Ansi(1))) == "0;31");
    assert(util::sgr_codes(cell_fg(Color::Ansi(7))) == "0;37");
    assert(util::sgr_codes(cell_fg(Color::Ansi(8))) == "0;90");
    assert(util::sgr_codes(cell_fg(Color::Ansi(15))) == "0;97");
}

void test_sgr_ansi_background_16() {
    assert(util::sgr_codes(cell_bg(Color::Ansi(4))) == "0;38;2;255;255;255;44");
    assert(util::sgr_codes(cell_bg(Color::Ansi(12))) ==
           "0;38;2;255;255;255;104");
}

void test_sgr_256() {
    assert(util::sgr_codes(cell_fg(Color::Ansi(16))) == "0;38;5;16");
    assert(util::sgr_codes(cell_fg(Color::Ansi(255))) == "0;38;5;255");
    assert(util::sgr_codes(cell_bg(Color::Ansi(200))) ==
           "0;38;2;255;255;255;48;5;200");
}

void test_sgr_default() {
    assert(util::sgr_codes(cell_fg(Color::Default())) == "0;39");
    assert(util::sgr_codes(cell_bg(Color::Default())) ==
           "0;38;2;255;255;255;49");
}

void test_sgr_transparent_bg_still_omitted() {
    assert(util::sgr_codes(cell_bg(Color(0, 0, 0, 0))) == "0;38;2;255;255;255");
}

void test_blend_opaque_indexed_src_wins() {
    auto r = renderer::blend(Color(10, 20, 30, 255), Color::Ansi(5));
    assert(r == Color::Ansi(5));
}

void test_blend_transparent_src_keeps_indexed_dst() {
    auto r = renderer::blend(Color::Ansi(5), Color(0, 0, 0, 0));
    assert(r == Color::Ansi(5));
}

void test_blend_indexed_dst_translucent_src() {
    // src can't be mixed with a palette color: >=128 alpha wins, else dst.
    Color translucent_hi(9, 9, 9, 200);
    Color translucent_lo(9, 9, 9, 50);
    assert(renderer::blend(Color::Ansi(5), translucent_hi) == translucent_hi);
    assert(renderer::blend(Color::Ansi(5), translucent_lo) == Color::Ansi(5));
}

void test_blend_rgb_unchanged() {
    auto r = renderer::blend(Color(0, 0, 0, 255), Color(200, 100, 50, 128));
    assert(r.is_rgb() && r.a == 255);
    assert(r.r > 90 && r.r < 110);
}

int main() {
    test::run("rgb_ctor", test_rgb_ctor_is_truecolor);
    test::run("ansi_ctor", test_ansi_ctor_is_indexed);
    test::run("kind_equality", test_default_ctor_and_kind_equality);
    test::run("sgr_rgb_unchanged", test_sgr_rgb_unchanged);
    test::run("sgr_ansi_16", test_sgr_ansi_16);
    test::run("sgr_ansi_background_16", test_sgr_ansi_background_16);
    test::run("sgr_256", test_sgr_256);
    test::run("sgr_default", test_sgr_default);
    test::run("sgr_transparent_bg", test_sgr_transparent_bg_still_omitted);
    test::run("blend_opaque_indexed_src", test_blend_opaque_indexed_src_wins);
    test::run("blend_transparent_src_keeps_dst",
              test_blend_transparent_src_keeps_indexed_dst);
    test::run("blend_indexed_dst_translucent_src",
              test_blend_indexed_dst_translucent_src);
    test::run("blend_rgb_unchanged", test_blend_rgb_unchanged);
    return test::summary();
}

